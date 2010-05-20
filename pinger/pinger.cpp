/*
	Код сделан на основе примера для boost.asio:

	ping.cpp
	~~~~~~~~
	
	Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
	
	Distributed under the Boost Software License, Version 1.0.
	(http://www.boost.org/LICENSE_1_0.txt)
*/

#include "pinger.h"
using namespace pinger;

#include "../common/my_time.h"
#include "../common/my_xml.h"
#include "../common/my_utf8.h"
#include "../common/my_http.h"
#include "../common/my_exception.h"

#include <istream>
#include <iostream>
#include <ostream>
#include <utility> /* std::pair */
using namespace std;

#include <boost/foreach.hpp>

posix_time::ptime pinger::now()
{
	return posix_time::microsec_clock::universal_time();
}

unsigned short pinger::get_id()
{
#if defined(BOOST_WINDOWS)
	return static_cast<unsigned short>(::GetCurrentProcessId());
#else
	return static_cast<unsigned short>(::getpid());
#endif
}

host_pinger::host_pinger(server &parent,
	asio::io_service &io_service,
	const std::wstring &hostname,
	icmp::endpoint endpoint,
	icmp::socket &socket,
	posix_time::time_duration timeout,
	posix_time::time_duration request_period,
	unsigned short max_results)
		: parent_(parent)
		, socket_(socket)
		, hostname_(hostname)
		, endpoint_(endpoint)
		, state_(host_state::unknown)
		, fails_(0)
		, timeout_(timeout)
		, request_period_(request_period)
		, results_(max_results)
		, sequence_number_(0)
		, timer_(io_service)
{
}

host_pinger_copy host_pinger::copy()
{
	scoped_lock l(pinger_mutex_); /* Блокируем пингер */
	return host_pinger_copy(*this);
}

void host_pinger::results_copy(vector<ping_result> &v)
{
	scoped_lock l(pinger_mutex_); /* Блокируем пингер */
	BOOST_FOREACH(ping_result &result, results_)
		v.push_back(result);
}

ping_result* host_pinger::find_result_(unsigned short sequence_number)
{
	scoped_lock l(pinger_mutex_); /* Блокируем пингер */

	BOOST_FOREACH(ping_result &result, results_)
		if (result.sequence_number == sequence_number)
			return &result;

	return NULL;
}

void host_pinger::run()
{
	static const string body("'Hello!' from Asio ping.");

	/* Создаём пакет */
	icmp_header echo_request;
	echo_request.type(icmp_header::echo_request);
	echo_request.code(0);
	echo_request.identifier(pinger::get_id());
	echo_request.sequence_number(++sequence_number_);
	compute_checksum(echo_request, body.begin(), body.end());

	asio::streambuf request_buffer;
	ostream os(&request_buffer);
	os << echo_request << body;

	/* Отправляем пакет */	
	last_ping_time_ = now();
	socket_.send_to(request_buffer.data(), endpoint_);

	timer_.expires_at( last_ping_time_ + timeout_ );
	timer_.async_wait( boost::bind(&pinger::host_pinger::handle_timeout_,
		this, sequence_number_) );
}

void host_pinger::handle_timeout_(unsigned short sequence_number)
{
	/* Сразу вычисляем время - избегаем лишних погрешностей */
	posix_time::ptime time = now();

	/* Вполне может случиться так, что сначала прийдёт отклик,
		но за время его обработки сработает таймаут и функция запустится.
		В этом случае ничего не делаем: результат ping'а уже в списке,
		таймер уже установлен */
	{
		scoped_lock l(pinger_mutex_); /* Блокируем пингер */

		ping_result *result_in_list = find_result_(sequence_number);
		if (result_in_list)
			return;
	}

	/* Сохраняем результат (отрицательный результат - тоже результат */
	ping_result result;
	result.sequence_number = sequence_number;
	result.state = ping_state::timeout;
	result.time_sent = last_ping_time_;
	result.time = time - last_ping_time_;

	{	
		scoped_lock l(pinger_mutex_); /* Блокируем пингер */
		results_.push_back(result);
		
		/* Мы имеем копию результата, поэтому дальнейшая
			блокировка нам не нужна */
	}
	
	/* Первый таймаут не считаем ошибочным.
		Сразу делаем ещё 3 дополнительных запроса */
	host_state old_state = state_;

	/* Фиксируем время переход в состояние таймаута */
	if (fails_ == 0)
		state_changed_ = time;

	if (++fails_ <= 4)
	{
		state_ = host_state::warn;
		timer_.expires_at(last_ping_time_ + timeout_);
	}
	else
	{
		state_ = host_state::fail;
		timer_.expires_at(last_ping_time_ + request_period_);
	}

	timer_.async_wait( boost::bind(&host_pinger::run, this) );

	/*TODO: Сохранение результата в БД */
	parent_.ping_notify(*this, result);
	if (old_state != state_)
		parent_.change_state_notify(*this);
}

void host_pinger::on_receive(posix_time::ptime time,
	const ipv4_header &ipv4_hdr, const icmp_header &icmp_hdr)
{
	ping_result *result_in_list = NULL;

	ping_result result;
	result.sequence_number = icmp_hdr.sequence_number();
	result.state = ping_state::ok;
	result.ipv4_hdr = ipv4_hdr;
	result.icmp_hdr = icmp_hdr;

	{
		scoped_lock l(pinger_mutex_); /* Блокируем пингер */

		/* При получении отклика, результат уже может быть в списке,
			если ранее сработал таймаут. В этом случае исправляем старый
			результат на новый */
		result_in_list = find_result_(result.sequence_number);

		if (result_in_list)
		{
			result.time_sent = result_in_list->time_sent;
			result.time = time - result.time_sent;
			*result_in_list = result;
		}
		else
		{
			result.time_sent = last_ping_time_;
			result.time = time - last_ping_time_;
			results_.push_back(result);
		}

		/* Мы имеем копию результата, поэтому дальнейшая
			блокировка нам не нужна */
	}

	if (!result_in_list)
	{
		timer_.cancel();
		timer_.expires_at( last_ping_time_ + request_period_ );
		timer_.async_wait( boost::bind(&host_pinger::run, this) );
	}

	host_state old_state = state_;

	if (fails_ != 0 || state_ == host_state::unknown)
		state_changed_ = time;

	if (sequence_number_ == result.sequence_number)
	{
		state_ = host_state::ok;
		fails_ = 0;
	}

	/*TODO: Сохранение результата в БД */
	parent_.ping_notify(*this, result);
	if (old_state != state_)
		parent_.change_state_notify(*this);
}

wstring host_pinger_copy::to_wstring() const
{
	wstringstream out;

	out << L"<state"
		<< L" address=\"" << my::ip::to_wstring(address) << L"\""
		<< L" state=\"" << state.to_wstring() << L"\""
		<< L" state_changed=\"" << my::time::to_wstring(state_changed) << L"\"";
		
	if (fails)
		out << L" fails=\"" << fails << L"\"";
		
	out << L"/>";

	return out.str();
}

wstring host_pinger_copy::result_to_wstring(const ping_result &result) const
{
	wstringstream out;

	out << L"<" << result.state.to_wstring()
		<< L" address=\"" << my::ip::to_wstring(address) << L"\""
		<< L" icmp_seq=\"" << result.sequence_number << L"\"";

	if (result.state == pinger::ping_state::ok)
		out << L" ttl=\"" << result.ipv4_hdr.time_to_live() << L"\"";

	out << L" start=\"" << my::time::to_wstring(result.time_sent) << L"\""
		<< L" time=\"" << result.time.total_milliseconds() << L"ms\""
		<< L"/>";

	return out.str();
}

server::server()
	: io_service_()
	, resolver_(io_service_)
	, socket_(io_service_, icmp::v4())
	, def_timeout_(0,0,1)
	, def_request_period_(0,0,5)
{
}

void server::run()
{
	receive_();
	io_service_.run();
}

void server::match(xml::wptree &pt)
{
	try
	{
		pair<xml::wptree::const_assoc_iterator,
			xml::wptree::const_assoc_iterator> p
				= pt.equal_range(L"host");

		while (p.first != p.second)
		{
			const xml::wptree &node = p.first->second;

			wstring hostname;
			posix_time::time_duration timeout(posix_time::not_a_date_time);
			posix_time::time_duration request_period(posix_time::not_a_date_time);

			/* Адрес - обязательно! */
			hostname = node.get_value<wstring>();

			wstring str;
			str = node.get<wstring>(L"<xmlattr>.timeout", L"");
			if (!str.empty())
				timeout = my::time::to_duration(str);
		
			str = node.get<wstring>(L"<xmlattr>.request_period", L"");
			if (!str.empty())
				request_period = my::time::to_duration(str);

			add_pinger(hostname, timeout, request_period);

			p.first++;
		}
	}
	catch(exception &e)
	{
		throw my::exception(e);
	}
}

icmp::endpoint server::resolve(const std::string &hostname)
{
	icmp::endpoint endpoint;

	try
	{
		icmp::resolver::query query(icmp::v4(), hostname, "");
		endpoint = *resolver_.resolve(query);
	}
	catch(std::exception &e)
	{
		throw my::exception(e)
			<< my::param(L"hostname", my::ip::punycode_decode(hostname));
	}

	return endpoint;
}

icmp::endpoint server::resolve(const std::wstring &hostname)
{
	icmp::endpoint endpoint;

	try
	{
		icmp::resolver::query query(icmp::v4(), my::ip::punycode_encode(hostname), "");
		endpoint = *resolver_.resolve(query);
	}
	catch(std::exception &e)
	{
		throw my::exception(e)
			<< my::param(L"hostname", hostname);
	}

	return endpoint;
}

bool server::add_pinger(const std::wstring &hostname,
	posix_time::time_duration timeout,
	posix_time::time_duration request_period)
{
	scoped_lock l(server_mutex_); /* Блокируем сервер */

	icmp::endpoint endpoint = resolve(hostname);

	/* Если уже существует - не добавляем */
	if ( find_pinger_( endpoint.address().to_v4() ) )
		return false;

	if (timeout.is_special())
		timeout = def_timeout_;
	
	if (request_period.is_special())
		request_period = def_request_period_;

	host_pinger *pinger = new host_pinger(*this, io_service_,
		hostname, endpoint, socket_, timeout, request_period);

	pingers_.push_back(pinger);

	pinger->run();

	return true;
}

host_pinger* server::find_pinger_(const ip::address_v4 &address,
	bool throw_if_not_found)
{
	scoped_lock l(server_mutex_); /* Блокируем сервер */

	BOOST_FOREACH(host_pinger &pinger, pingers_)
		if (pinger.address() == address)
			return &pinger;

	if (throw_if_not_found)
		throw my::exception(L"Адрес не найден")
			<< my::param(L"address", address);

	return NULL;
}

void server::receive_()
{
	/* Discard any data already in the buffer */
	reply_buffer_.consume(reply_buffer_.size());

	/* Wait for a reply. We prepare the buffer to receive up to 64KB */
	socket_.async_receive(reply_buffer_.prepare(65536),
		boost::bind(&server::handle_receive_, this, _2));
}

void server::handle_receive_(size_t length)
{
	/* Сразу вычисляем время - избегаем лишних погрешностей */
	posix_time::ptime time = now();

	/* The actual number of bytes received is committed to the buffer
		so that we can extract it using a istream object */
	reply_buffer_.commit(length);

	/* Decode the reply packet */
	istream is(&reply_buffer_);
	ipv4_header ipv4_hdr;
	icmp_header icmp_hdr;
	is >> ipv4_hdr >> icmp_hdr;

	/* We can receive all ICMP packets received by the host, so we need to
		filter out only the echo replies that match the our identifier */
	if ( is && icmp_hdr.type() == icmp_header::echo_reply
		&& icmp_hdr.identifier() == pinger::get_id() )
	{
		scoped_lock l(server_mutex_); /* Блокируем сервер */

		/* Проверяем, что адрес есть в списке */
		host_pinger *host_pinger =
			find_pinger_(ipv4_hdr.source_address());

		if (host_pinger)
			host_pinger->on_receive(time, ipv4_hdr, icmp_hdr);
	}

	receive_();
}

void server::pingers_copy(std::vector<host_pinger_copy> &v)
{
	scoped_lock l(server_mutex_); /* Блокируем сервер */

	BOOST_FOREACH(host_pinger &pinger, pingers_)
		v.push_back( pinger.copy() );
}
