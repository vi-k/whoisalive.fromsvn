#include "acceptor.h"

#include "../common/my_time.h"
#include "../common/my_xml.h"
#include "../common/my_http.h"
#include "../common/my_num.h"
#include "../common/my_exception.h"

#include <string>
#include <sstream>
#include <locale>
#include <exception>
#include <utility>
using namespace std;

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

using namespace acceptor;

server::server(xml::wptree &config)
	: io_service_()
	, acceptor_(io_service_)
	, pinger_()
	, state_eventer_()
	, ping_eventer_()
{
	tcp::resolver resolver(io_service_);

	wstring address = config.get<wstring>(L"address",L"");
	wstring port = config.get<wstring>(L"port", L"");

	if (port.empty())
		throw my::exception(L"Не задан порт");

	tcp::endpoint endpoint;

	try
	{
		if (address.empty())
			endpoint = tcp::endpoint(tcp::v4(), my::num::to_int(port, 0));
		else
		{
			tcp::resolver::query query(
				my::ip::punycode_encode(address), my::str::to_string(port) );
			endpoint = *resolver.resolve(query);
		}

		acceptor_.open(endpoint.protocol());
		acceptor_.set_option(tcp::acceptor::reuse_address(true));
		acceptor_.bind(endpoint);
		acceptor_.listen();
	}
	catch(std::exception &e)
	{
		throw my::exception(e)
			<< my::param(L"address", address)
			<< my::param(L"port", port);
	}

	pinger_.set_def_timeout(
		my::time::to_duration(config.get<wstring>(L"def_timeout")) );
	pinger_.set_def_request_period(
		my::time::to_duration(config.get<wstring>(L"def_request_period")) );
	pinger_.on_change_state = boost::bind(&server::on_change_state, this, _1);
	pinger_.on_ping = boost::bind(&server::on_ping, this, _1, _2);

	try
	{
		xml::wptree pt;
		my::xml::load(L"maps/maps.xml", pt);

		pair<xml::wptree::assoc_iterator,
			xml::wptree::assoc_iterator> p
				= pt.get_child(L"maps").equal_range(L"map");

		while (p.first != p.second)
		{
			wstring id = p.first->second.get<wstring>(L"id");
			map_info mi;
			mi.tile_type = p.first->second.get<wstring>(L"tile-type");
		
			if (mi.tile_type == L"image/jpeg")
				mi.ext = L"jpg";
			else if (mi.tile_type == L"image/png")
				mi.ext = L"png";
			else
				throw my::exception(L"Неизвестный тип тайла")
					<< my::param(L"tile-type", mi.tile_type);

			maps_[id] = mi;
			p.first++;
		}
	}
	catch(my::exception &e)
	{
		throw my::exception(L"Не удалось загрузить описание карт")
			<< my::param(L"file", L"maps/maps.xml")
			<< e;
	}
	catch(std::exception &e)
	{
		throw my::exception(L"Не удалось загрузить описание карт")
			<< my::param(L"file", L"maps/maps.xml")
			<< e;
	}
}

void server::run()
{
	try
	{
		xml::wptree pt;
		my::xml::load(L"hosts.xml", pt);
		pinger_.match( pt.get_child(L"hosts") );
	}
	catch(my::exception &e)
	{
		throw my::exception(L"Ошибка загрузки настроек")
			<< my::param(L"file", L"hosts.xml")
			<< e;
	}
	catch(exception &e)
	{
		throw my::exception(L"Ошибка загрузки настроек")
			<< my::param(L"file", L"hosts.xml")
			<< e;
	}

	thgroup_.create_thread(
		boost::bind(&pinger::server::run, &pinger_) );

	thgroup_.create_thread(
		boost::bind(&eventer::server::run, &state_eventer_) );

	thgroup_.create_thread(
		boost::bind(&eventer::server::run, &ping_eventer_) );

	accept();

	io_service_.run();
}

void server::accept(void)
{
	new_connection_.reset( new connection(*this) );

	acceptor_.async_accept( new_connection_->socket(),
		boost::bind( &server::handle_accept, this,
			asio::placeholders::error) );
}

void server::handle_accept(const boost::system::error_code &e)
{
    if (!e)
	{
		thgroup_.create_thread( boost::bind(&connection::run,
			new_connection_.release()) );

		accept();
    }
}

void server::on_change_state(const pinger::host_pinger_copy &pinger)
{
	wstring str = pinger.to_wstring();
#ifdef _DEBUG
	wcout << str << endl;
#endif
	state_eventer_.add_event(pinger.address, str);
}

void server::on_ping(const pinger::host_pinger_copy &pinger,
	const pinger::ping_result &result)
{
#ifdef _DEBUG
	wcout << pinger.result_winfo(result) << endl;
#endif
	ping_eventer_.add_event(pinger.address, pinger.result_to_wstring(result));
}
