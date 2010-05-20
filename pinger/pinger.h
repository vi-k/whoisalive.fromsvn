#ifndef PINGER_H
#define PINGER_H

/*
	Код создан на основе примера для boost.asio:

	ping.cpp, ipv4_header.hpp, icmp_header.hpp
	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
	
	Distributed under the Boost Software License, Version 1.0. (See accompanying
	file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "../common/my_thread.h"
#include "../common/my_inet.h"
#include "../common/my_time.h"
#include "../common/my_xml.h"

#include <vector>

#include <boost/bind.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/ptr_container/ptr_list.hpp>

#include "icmp_header.hpp"
#include "ipv4_header.hpp"

namespace pinger {

inline posix_time::ptime now();
inline unsigned short get_id();

class ping_state
{
public:
	enum type {unknown, ok, timeout};
private:
	type state_;
public:
	ping_state(type state)
		: state_(state) {};

	std::string to_string() const
	{
		return state_ == ping_state::ok ? "reply"
			: state_ == ping_state::timeout ? "timeout"
			: "unknown";
	}

	std::wstring to_wstring() const
	{
		return state_ == ping_state::ok ? L"reply"
			: state_ == ping_state::timeout ? L"timeout"
			: L"unknown";
	}

	inline bool operator ==(const ping_state &rhs) const
		{ return state_ == rhs.state_; }
	inline bool operator !=(const ping_state &rhs) const
		{ return state_ != rhs.state_; }
};

/* Результат отдельного ping'а */
struct ping_result
{
	unsigned short sequence_number;
	ping_state state;
	posix_time::ptime time_sent;
	posix_time::time_duration time;
	ipv4_header ipv4_hdr;
	icmp_header icmp_hdr;

	ping_result()
		: sequence_number(0)
		, state(ping_state::unknown) {}
};

class host_state
{
public:
	enum type {unknown, ok, warn, fail};
private:
	type state_;
public:
	host_state(type state)
		: state_(state) {};

	std::string to_string() const
	{
		return state_ == host_state::ok ? "ok"
			: state_ == host_state::warn ? "warn"
			: state_ == host_state::fail ? "fail"
			: "unknown";
	}
	std::wstring to_wstring() const
	{
		return state_ == host_state::ok ? L"ok"
			: state_ == host_state::warn ? L"warn"
			: state_ == host_state::fail ? L"fail"
			: L"unknown";
	}

	inline bool operator ==(const host_state &rhs) const
		{ return state_ == rhs.state_; }
	inline bool operator !=(const host_state &rhs) const
		{ return state_ != rhs.state_; }
};

/* Пингер отдельного хоста */
class server;
class host_pinger
{
	friend class host_pinger_copy;

private:
	server &parent_;
	std::wstring hostname_; /* Имя хоста */
	icmp::endpoint endpoint_; /* ip-адрес */
	host_state state_; /* Состояние адреса */
	posix_time::ptime state_changed_; /* Время изменения состояния */
	int fails_; /* Кол-во таймаутов подряд */
	boost::circular_buffer<ping_result> results_; /* Результаты пингов */
	unsigned short sequence_number_; /* Номер последнего пинга */
	posix_time::ptime last_ping_time_; /* Время отправки последнего пинга */
	icmp::socket &socket_;
	asio::deadline_timer timer_;
	posix_time::time_duration timeout_; /* Время ожидания ответа */
	posix_time::time_duration request_period_; /* Период опроса */
	mutex pinger_mutex_; /* Блокировка всего пингера */

	ping_result* find_result_(unsigned short sequence_number);
	void handle_timeout_(unsigned short sequence_number);
	
public:
	host_pinger(server &parent,
		asio::io_service &io_service /* Для таймера */,
		const std::wstring &hostname,
		icmp::endpoint endpoint,
		icmp::socket &socket,
		posix_time::time_duration timeout,
		posix_time::time_duration request_period,
		unsigned short max_results = 100);

	void run();

	void on_receive(posix_time::ptime time,
		const ipv4_header &ipv4_hdr, const icmp_header &icmp_hdr);

	
	/* Чтение параметров пингера */
	ip::address_v4 address()
	{
		scoped_lock l(pinger_mutex_); /* Блокируем пингер */
		return endpoint_.address().to_v4();
	}

	class host_pinger_copy copy();

	void results_copy(std::vector<ping_result> &v);

#if 0
	host_state::type state()
	{
		scoped_lock l(pinger_mutex_); /* Блокируем пингер */
		return state_;
	}

	posix_time::ptime state_changed()
	{
		scoped_lock l(pinger_mutex_); /* Блокируем пингер */
		return state_changed_;
	}
	
	int fails()
	{
		scoped_lock l(pinger_mutex_); /* Блокируем пингер */
		return fails_;
	}

	posix_time::time_duration timeout()
	{
		scoped_lock l(pinger_mutex_);
		return timeout_;
	}

	inline posix_time::time_duration request_period()
	{
		scoped_lock l(pinger_mutex_);
		return request_period_;
	}
#endif
	
	/* Изменение параметров пингера */

	void set_request_period(posix_time::time_duration request_period)
	{
		scoped_lock l(pinger_mutex_);
		request_period_ = request_period;
	}

	void set_timeout(posix_time::time_duration timeout)
	{
		scoped_lock l(pinger_mutex_);
		timeout_ = timeout;
	}
};

/* Структура для копирования и передачи сведений о host_pinger'е */
class host_pinger_copy
{
public:
	std::wstring hostname;
	ip::address_v4 address;
	host_state state;
	posix_time::ptime state_changed;
	int fails;
	posix_time::time_duration timeout;
	posix_time::time_duration request_period;

	host_pinger_copy(host_pinger &pinger)
		: hostname(pinger.hostname_)
		, address(pinger.endpoint_.address().to_v4())
		, state(pinger.state_)
		, state_changed(pinger.state_changed_)
		, fails(pinger.fails_)
		, timeout(pinger.timeout_)
		, request_period(pinger.request_period_) {}

	std::wstring to_wstring() const;
	std::wstring result_to_wstring(const ping_result &result) const;
};

/* Пингер-сервер */
class server
{
private:
	asio::io_service io_service_;
	icmp::resolver resolver_;
	boost::ptr_list<host_pinger> pingers_;
	icmp::socket socket_;
	asio::streambuf reply_buffer_;
	posix_time::time_duration def_timeout_;
	posix_time::time_duration def_request_period_;
	mutex server_mutex_;

	void receive_();
	void handle_receive_(std::size_t length);
	host_pinger* find_pinger_(const ip::address_v4 &address,
		bool throw_if_not_found = false);

public:
	server();

	boost::function<void (const host_pinger_copy &pinger)> on_change_state;
	boost::function<void (const host_pinger_copy &pinger,
		const ping_result &result)> on_ping;

	void run();

	/* Разобрать настройки */
	void match(xml::wptree &pt);

	icmp::endpoint resolve(const std::string &hostname);
	icmp::endpoint resolve(const std::wstring &hostname);

	bool add_pinger(const std::wstring &hostname,
		posix_time::time_duration timeout = posix_time::not_a_date_time,
		posix_time::time_duration request_period = posix_time::not_a_date_time);

	void change_state_notify(host_pinger &pinger)
		{ if (on_change_state) on_change_state(pinger.copy()); }
	void ping_notify(host_pinger &pinger, const ping_result &result)
		{ if (on_ping) on_ping(pinger.copy(), result); }

	void pingers_copy(std::vector<host_pinger_copy> &v);
	
	host_pinger_copy pinger_copy(const ip::address_v4 &address)
	{
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		host_pinger *pinger = find_pinger_(address, true);
		return host_pinger_copy(*pinger);
	}

	void results_copy(const ip::address_v4 &address,
		std::vector<ping_result> &v)
	{
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		host_pinger *pinger = find_pinger_(address, true);
		pinger->results_copy(v);
	}
	
	/* Чтение параметров сервера */
	
	inline posix_time::time_duration def_timeout()
	{
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		return def_timeout_;
	}

	inline posix_time::time_duration def_request_period()
	{
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		return def_request_period_;
	}

	
	/* Изменение параметров сервера */

	void set_def_timeout(posix_time::time_duration time)
	{
		my::time::throw_if_fail(time);
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		def_timeout_ = time;
	}

	void set_def_request_period(posix_time::time_duration time)
	{
		my::time::throw_if_fail(time);
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		def_request_period_ = time;
	}


	/* Чтение параметров пингеров */
#if 0
	posix_time::time_duration timeout(const ip::address_v4 &address)
	{
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		host_pinger *pinger = find_pinger_(address, true /*throw_if_not_found*/);
		return pinger->timeout();
	}

	posix_time::time_duration request_period(const ip::address_v4 &address)
	{
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		host_pinger *pinger = find_pinger_(address, true /*throw_if_not_found*/);
		return pinger->request_period();
	}
#endif

	/* Изменение параметров пингеров */

	void set_timeout(const ip::address_v4 &address,
		posix_time::time_duration time)
	{
		my::time::throw_if_fail(time);
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		host_pinger *pinger = find_pinger_(address, true /*throw_if_not_found*/);
		pinger->set_timeout(time);
	}

	void set_request_period(const ip::address_v4 &address,
		posix_time::time_duration time)
	{
		my::time::throw_if_fail(time);
		scoped_lock l(server_mutex_); /* Блокируем сервер */
		host_pinger *pinger = find_pinger_(address, true /*throw_if_not_found*/);
		pinger->set_request_period(time);
	}
};

}

#endif
