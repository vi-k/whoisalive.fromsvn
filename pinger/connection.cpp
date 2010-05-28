#include "connection.h"
#include "acceptor.h"

#include "../common/my_xml.h"
#include "../common/my_utf8.h"
#include "../common/my_time.h"
#include "../common/my_http.h"
#include "../common/my_str.h"
#include "../common/my_num.h"
#include "../common/my_fs.h"
#include "../common/my_exception.h"
#include "../common/my_log.h"
extern my::log main_log;

#define MY_STOPWATCH_DEBUG
#include "../common/my_debug.h"
MY_STOPWATCH( __proc_sw(my::stopwatch::show_all & ~my::stopwatch::show_total) )

#include <sstream>
#include <fstream>
#include <locale>
#include <exception>
#include <map>
using namespace std;

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

using namespace acceptor;

connection::connection(class server &server)
	: server_(server)
	, socket_(server_.io_service())
{
}

connection::~connection()
{
	socket_.close();
}

void connection::run()
{
	MY_STOPWATCH_START(__proc_sw)

	try
	{
		/* Читаем запрос */
		my::http::request request;

		request.read_request(socket_);
		request.read_header(socket_);

		fs::wpath url(request.url);

		wstring dir = url.parent_path().string();
		wstring file = url.filename();
		wstring ext = url.extension();
		if (!ext.empty())
			ext = ext.substr(1); /* Убираем точку - она нам не нужна */

		bool parsed = true;

		if (url == L"/favicon.ico")
		{
			if (!send_file(L"favicon.ico", "image/x-icon"))
				parsed = false;
		}

		else if (url == L"/schemes.xml")
		{
			xml::wptree pt;
			my::xml::load(L"schemes.xml", pt);
			send_xml(pt);
		}

		/* Карты */
		else if (dir == L"/maps")
		{
			if (file == L"maps.xml")
			{
				xml::wptree pt;
				my::xml::load(L"maps/maps.xml", pt);
				send_xml(pt);
			}

			else if (file == L"gettile")
			{
				wstring map = request.params[L"map"];
				map_info_map &maps = server_.maps();
				map_info_map::iterator it = maps.find(map);

				if (it == maps.end())
					throw my::exception(L"Карта не найдена")
						<< my::param(L"request", my::str::to_wstring(request.request_));

				int z = my::num::to_int(request.params[L"z"], -1);
				int x = my::num::to_int(request.params[L"x"], -1);
				int y = my::num::to_int(request.params[L"y"], -1);

				int maxc = 0;
				if (z >= 1 && z <= 25)
					maxc = 1 << (z - 1);
				if (x < 0 || x >= maxc || y < 0 || y >= maxc)
					throw my::exception(L"Вне допустимого диапазона")
						<< my::param(L"request", my::str::to_wstring(request.request_));

				wstringstream filename;
				filename << L"maps/" << map
					<< L"/z" << z
					<< L"/" << (x >> 10) << L"/x" << x
					<< L"/" << (y >> 10) << L"/y" << y
					<< L"." << it->second.ext;

				if (!send_file(filename.str(), my::str::to_string(it->second.tile_type)))
						parsed = false;
			}

			else
				parsed = false;

		} /* if (dir == L"/maps") */

		/* Классы объектов */
		else if (dir == L"/classes")
		{
			/* Список классов */
			if (file == L"classes.xml")
			{
				xml::wptree pt;
				my::xml::load(L"classes/classes.xml", pt);
				send_xml(pt);
			}

			/* Изображения для классов */
			else if(ext == L"png" || ext == L"jpg" || ext == L"png")
			{
				wstring filename = L"classes/" + file;
				string content_type = "image/" + my::str::to_string(ext);

				if (!send_file(filename, content_type))
					parsed = false;
			}

			else
				parsed = false;

		} /* else if (dir == L"/classes") */

		/* Пингер */
		else if (dir == L"/pinger")
		{
			/* Журнал пингов. Если указан address - только
				одного хоста. Перед началом ведения журнала отдаются ранее
				сохранённый архив результатов. Журнал отдаётся бесконечно */
			if (file == L"ping.log")
			{
				string address_s = my::ip::punycode_encode(request.params[L"address"]);
				ip::address_v4 address;

				if (!address_s.empty())
					address = server_.pinger().resolve(address_s).address().to_v4();

				/* Блокируем отправку событий, пока
					не передадим архив */
				scoped_lock lock( server_.ping_eventer().get_lock() );

				wstringstream out;

				if (address != ip::address_v4::any())
				{
					/* Только указанный адрес */
					pinger::host_pinger_copy pinger
						= server_.pinger().pinger_copy(address);

					vector<pinger::ping_result> results;
					server_.pinger().results_copy(address, results);

					out << L"START ARCHIVE\r\n";

					BOOST_REVERSE_FOREACH(pinger::ping_result &result, results)
						out << pinger.result_to_wstring(result)
							<< L"\r\n";

					out << L"END ARCHIVE\r\n";
				}
				else
				{
					/* Все адреса */
					vector<pinger::host_pinger_copy> pingers;
					server_.pinger().pingers_copy(pingers);

					BOOST_REVERSE_FOREACH(pinger::host_pinger_copy &pinger, pingers)
					{
						vector<pinger::ping_result> results;
						server_.pinger().results_copy(pinger.address, results);

						out << L"START ARCHIVE\r\n";

						BOOST_FOREACH(pinger::ping_result &result, results)
							out << pinger.result_to_wstring(result)
								<< L"\r\n";
						
						out << L"END ARCHIVE\r\n";
					}
				}

				send_ok("text/plain; charset=utf-8");
				send_in_utf8(out.str());

				server_.ping_eventer().add_handler(this, address);

				/* Здесь нам больше делать нечего, но соединение не закрываем.
					Оставляем это дело eventer'у */
				return;

			} /* if (file == L"ping.log") */
		
			/* Журнал состояния хостов. Если указан address - только
				одного хоста. Перед началом отдачи журнала отдаются текущие
				состояния. Журнал отдаётся бесконечно */
			else if (file == L"state.log")
			{
				string address_s = my::ip::punycode_encode(request.params[L"address"]);
				ip::address_v4 address;

				if (!address_s.empty())
					address = server_.pinger().resolve(address_s).address().to_v4();

				/* Блокируем отправку событий, пока
					не передадим текущее состояние */
				scoped_lock lock( server_.state_eventer().get_lock() );

				wstringstream out;

				if (address != ip::address_v4::any())
				{
					/* Только указанный адрес */
					pinger::host_pinger_copy pinger
						= server_.pinger().pinger_copy(address);

					out << pinger.to_wstring() << L"\r\n";
				}
				else
				{
					/* Все адреса */
					vector<pinger::host_pinger_copy> pingers;
					server_.pinger().pingers_copy(pingers);

					BOOST_FOREACH(pinger::host_pinger_copy &pinger, pingers)
						out << pinger.to_wstring() << L"\r\n";
				}

				send_ok("text/plain; charset=utf-8");
				send_in_utf8(out.str());

				server_.state_eventer().add_handler(this, address);

				/* Здесь нам больше делать нечего, но соединение не закрываем.
					Оставляем это дело eventer'у */
				return;

			} /* else if (file == L"state.log") */

			else if (file == L"params.xml")
			{
				xml::wptree pt;
				xml::wptree &node = pt.put(L"params", L"");

				wstring hostname = request.params[L"host"];

				/*-
				if (hostname.empty())
				{
					string str = params["def_timeout"];
					if (!str.empty())
						server_.pinger().set_def_timeout(
							posix_time::duration_from_string(str));

					str = params["def_request_period"];
					if (!str.empty())
						server_.pinger().set_def_request_period(
							posix_time::duration_from_string(str));

					node.put( L"def_timeout",
						my::time::to_wstring(server_.pinger().def_timeout()) );
					node.put( L"def_request_period",
						my::time::to_wstring(server_.pinger().def_request_period()) );
				}
				else {
					ip::address_v4 address
						= server_.pinger().resolve(hostname).address().to_v4();

					xml::wptree &attr = node.put(L"<xmlattr>", L"");
					attr.put(L"address", address.to_string().c_str());

					string str = params["timeout"];
					if (!str.empty())
						server_.pinger().set_timeout( address,
							posix_time::duration_from_string(str) );

					str = params["request_period"];
					if (!str.empty())
						server_.pinger().set_request_period( address,
							posix_time::duration_from_string(str) );

					node.put( L"timeout",
						my::time::to_wstring(server_.pinger().timeout(address)) );
					node.put( L"request_period",
						my::time::to_wstring(server_.pinger().request_period(address)) );
				}
				-*/

				send_xml(pt);

			} /* else if (file == L"params.xml") */
		
			/* Состояние хостов */
			else if (file == L"state.xml")
			{
				xml::wptree pt;
				xml::wptree &root = pt.put(L"hosts", L"");

				vector<pinger::host_pinger_copy> pingers;
				server_.pinger().pingers_copy(pingers);
				posix_time::time_duration def_timeout
					= server_.pinger().def_timeout();
				posix_time::time_duration def_request_period
					= server_.pinger().def_request_period();

				BOOST_FOREACH(pinger::host_pinger_copy &pinger, pingers)
				{
					xml::wptree &node = root.add(L"host", L"");
					xml::wptree &attr = node.add(L"<xmlattr>", L"");
					wstring address = my::ip::to_wstring(pinger.address);
					attr.put(L"address", address);
					if (address != pinger.hostname)
						attr.put(L"hostname", pinger.hostname);
					attr.put(L"state", pinger.state.to_wstring());
					attr.put(L"state_changed", my::time::to_wstring(pinger.state_changed));
					if (pinger.fails)
						attr.put(L"fails", pinger.fails);
					if (pinger.timeout != def_timeout)
						attr.put(L"timeout", my::time::to_wstring(pinger.timeout));
					if (pinger.request_period != def_request_period)
						attr.put(L"request_period", my::time::to_wstring(pinger.request_period));
				}

				send_xml(pt);

			} /* else if (file == L"state.xml") */

			/* Параметры хостов - конфигурационный файл */
			else if (file == L"hosts.xml")
			{
				xml::wptree pt;
				my::xml::load(L"hosts.xml", pt);

				server_.pinger().match( pt.get_child(L"hosts") );

				send_xml(pt);

			} /* else if (file == L"hosts.xml") */

			else if (file == L"add")
			{
				wstring hostname = request.params[L"address"];

				if (hostname.empty())
					throw my::exception(L"Не указан адрес")
						<< my::param(L"request", my::str::to_wstring(request.request_));

				posix_time::time_duration timeout(posix_time::not_a_date_time);
				wstring str = request.params[L"timeout"];
				if (!str.empty())
					timeout = my::time::to_duration(str);

				posix_time::time_duration request_period(posix_time::not_a_date_time);
				str = request.params[L"request_period"];
				if (!str.empty())
					request_period = my::time::to_duration(str);

				server_.pinger().add_pinger(hostname, timeout, request_period);

			} /* else if (file == L"add") */

			else
				parsed = false;

		} /* else if (dir == L"/pinger") */

		else
			parsed = false;

		if (!parsed)
			throw my::exception(L"По данному запросу нет данных")
				<< my::param(L"request", my::str::to_wstring(request.request_));
		
		/* Удаляем себя */
		delete this;
	}
	catch (my::exception &e)
	{
		wstring error = e.message();
		main_log << L"my::exception in connection\n"
			<< error << main_log;
		send_404(error);
		delete this;
	}
	catch (exception &e)
	{
		my::exception my_e(e);
		wstring error = my_e.message();
		main_log << L"std::exception in connection\n"
			<< error << main_log;
		send_404(error);
		delete this;
    }
	catch (...)
	{
		wstring str(L"unexpected exception in connection");
		main_log << str << main_log;
		send_404(str);
		delete this;
	}

	MY_STOPWATCH_FINISH(__proc_sw)
	MY_STOPWATCH_OUT(main_log,
		L"connection::run() " << __proc_sw << main_log)
}

void connection::send_header(unsigned int status_code,
	const string &status_message, const string &content_type)
{
	stringstream out;

	out << "HTTP/1.1 "<< status_code << " " << status_message << "\r\n"
		<< "Content-Type: " << content_type << "\r\n"
		<< "\r\n";

	send(out.str());
}

void connection::send_404(const std::wstring &message)
{
	try
	{
		send_header(404, "Page Not Found", "text/plain; charset=utf-8");
		send_in_utf8(message);
	}
	catch(...)
	{
	}
}

bool connection::send_file(const wstring &filename,
	const string &content_type)
{
	ifstream fs(filename.c_str(), ios_base::binary);
	if (!fs)
		return false;

	send_ok(content_type);

	char ch;
	string data;

	while (fs.get(ch))
		data += ch;

	send(data);

	return true;
}

void connection::send_xml(xml::wptree &pt)
{
	/* Минимизируем xml */
	xml::xml_writer_settings<wchar_t> xs(L' ', 0, L"utf-8");

	wstringstream out;
	write_xml(out, pt, xs);

	send_ok("application/xml; charset=utf-8");
	send_in_utf8(out.str());
}
