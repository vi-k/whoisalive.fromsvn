#include "server.h"

#include "../common/my_inet.h"
#include "../common/my_http.h"
#include "../common/my_utf8.h"
#include "../common/my_fs.h"
#include "../common/my_log.h"
extern my::log main_log;

#define MY_STOPWATCH_DEBUG
#include "../common/my_debug.h"
MY_STOPWATCH( __load_file_sw1(my::stopwatch::show_all & ~my::stopwatch::show_total) )
MY_STOPWATCH( __load_file_sw2(my::stopwatch::show_all & ~my::stopwatch::show_total) )

#include <sstream>
#include <iostream>
#include <map>
#include <utility> /* std::pair */
using namespace std;

#include <boost/config/warning_disable.hpp> /* против unsafe */
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#pragma warning(disable:4355) /* 'this' : used in base member initializer list */

namespace who {

server::server(const xml::wptree &config)
	: terminate_(false)
	, gdiplus_token_(0)
	, io_service_()
	, state_log_socket_(io_service_)
	, anim_period_( posix_time::milliseconds(40) )  /* 20 40 200 */
	, def_anim_steps_(5)                                  /* 10  5   1 */
	, active_map_id_(1)
	, tiler_(*this, 1000, boost::bind(&server::on_tiler_update, this))
{
	/* Инициализируем GDI+ */
	Gdiplus::GdiplusStartupInput gs;
	Gdiplus::GdiplusStartup(&gdiplus_token_, &gs, NULL);

	tcp::resolver resolver(io_service_);
	tcp::resolver::query query(
		my::ip::punycode_encode(config.get<wstring>(L"address",L"")),
		my::str::to_string(config.get<wstring>(L"port",L"")) );
	server_endpoint_ = *resolver.resolve(query);

	load_classes_();
	load_maps_();

	/* Потока для приёма журнала состояний хостов */
	state_log_thread_ = boost::thread(
		boost::bind(&server::state_log_thread_proc, this) );

	/* Поток работы с io_service */
	io_thread_ = boost::thread(
		boost::bind(&server::io_thread_proc, this) );
}

void server::io_thread_proc()
{
	mutex io_mutex;

	while (!terminate_)
	{
		io_service_.run();
		io_service_.reset();

		if (terminate_)
			break;

		scoped_lock lock(io_mutex);
		io_cond_.wait(lock);
	}
}

server::~server()
{
	terminate_ = true;

	windows_.clear();
	classes_.clear();

	//TODO: (ptr_list-объекты будут очищены после, так что здесь нельзя закрывать Gdi+)
	//Gdiplus::GdiplusShutdown(gdiplus_token_);

	io_wake_up();
	io_thread_.join();
	
	state_log_socket_.close();
	state_log_thread_.join();
}

void server::state_log_thread_proc( void)
{
	try
	{
		my::http::reply reply;
		get_header(state_log_socket_, reply, L"/pinger/state.log");

		if (reply.status_code != 200)
			throw my::exception(L"Сервер не вернул состояние хостов")
				<< my::param(L"http-status-code", reply.status_code)
				<< my::param(L"http-status-message", reply.status_message);
	
		while (!terminate_)
		{
			size_t n = asio::read_until(state_log_socket_, reply.buf_, "\r\n");

			if (terminate_)
				break;

			reply.body.resize(n);
			reply.buf_.sgetn((char*)reply.body.c_str(), n);

			xml::wptree pt;
			reply.to_xml(pt);

			wstring value = pt.get<wstring>(L"state.<xmlattr>.address");
			ipaddr_t address(value.c_str());

			ipstate::t state;
			value = pt.get<wstring>(L"state.<xmlattr>.state");
			if (value == L"ok") state = ipstate::ok;
			else if (value == L"warn") state = ipstate::warn;
			else if (value == L"fail") state = ipstate::fail;
			else state = ipstate::unknown;

			if ( ipaddrs_[address].set_state(state) )
				check_state_notify();
		}
	}
	catch (my::exception &e)
	{
		if (!terminate_)
			throw e;
	}
	catch(exception &e)
	{
		if (!terminate_)
			throw my::exception(e)
				<< my::param(L"where", L"who::server::state_log_thread_proc()");
	}
}

/******************************************************************************
* Загрузка классов
*/
void server::load_classes_(void)
{
	try
	{
		my::http::reply reply;
		get(reply, L"/classes/classes.xml");

		xml::wptree pt;
		reply.to_xml(pt);
	
		pair<xml::wptree::assoc_iterator,
			xml::wptree::assoc_iterator> p
				= pt.get_child(L"classes").equal_range(L"class");

		/* Создаём классы */
		while (p.first != p.second)
		{
			wstring class_name = p.first->second.get<wstring>(L"<xmlattr>.name");
			try
			{
				classes_[class_name].reset(
					new who::obj_class(*this, p.first->second) );
			}
			catch(my::exception &e)
			{
				throw e << my::param(L"class-name", class_name);
			}

			p.first++;
		}
	}
	catch(my::exception &e)
	{
		throw my::exception(L"Ошибка загрузки классов")
			<< my::param(L"file", L"/classes/classes.xml")
			<< e;
	}
}

void server::load_maps_(void)
{
	try
	{
		my::http::reply reply;
		get(reply, L"/maps/maps.xml");

		xml::wptree pt;
		reply.to_xml(pt);
	
		pair<xml::wptree::assoc_iterator,
			xml::wptree::assoc_iterator> p
				= pt.get_child(L"maps").equal_range(L"map");

		while (p.first != p.second)
		{
			tiler::map map;
			map.id = p.first->second.get<wstring>(L"id");
			map.name = p.first->second.get<wstring>(L"name", L"");
			map.is_layer = p.first->second.get<bool>(L"layer", 0);
			
			map.tile_type = p.first->second.get<wstring>(L"tile-type");
			if (map.tile_type == L"image/jpeg")
				map.ext = L"jpg";
			else if (map.tile_type == L"image/png")
				map.ext = L"png";
			else
				throw my::exception(L"Неизвестный тип тайла")
					<< my::param(L"tile-type", map.tile_type);

			wstring projection = p.first->second.get<wstring>(L"projection");
			if (projection == L"wgs84")
				map.projection = tiler::map::wgs84;
			else if (projection == L"spheroid")
				map.projection = tiler::map::spheroid;
			else
				throw my::exception(L"Неизвестный тип проекции")
					<< my::param(L"projection", map.projection);

			tiler_.add_map(map);
			p.first++;
		}
	}
	catch(my::exception &e)
	{
		throw my::exception(L"Ошибка загрузки описания карт")
			<< my::param(L"file", L"/maps/maps.xml")
			<< e;
	}
}

/******************************************************************************
* Добавление окна
*/
window* server::add_window(HWND parent_wnd)
{
	window *win = new window(*this, parent_wnd);
	windows_.push_back(win);

	check_state_notify();

	return win;
}

/* Регистрация ip-адреса */
void server::register_ipaddr(const ipaddr_t &addr)
{
	/* Увеличиваем счётчик, если не было адреса - будет создан */
	ipaddrs_[addr].add_user();

	/* Добавляем на сервер */
	add_addr_(addr);
}

/* Снятие регистрации ip-адреса */
void server::unregister_ipaddr(const ipaddr_t &addr)
{
	/* Уменьшаем счётчик, если больше нет пользователей, удаляем адрес из списка */
	if ( ipaddrs_[addr].del_user() == 0 )
		ipaddrs_.erase( ipaddrs_.find(addr) );
}

bool server::add_addr_(const ipaddr_t &ipaddr)
{
	/* Регистрация на сервере */
	/*-
	wstringstream out;
	out << "/pinger/add?address=" << ipaddr;
	wstring res = query(out.str().c_str());
	if (res != L"OK") return false;
	-*/
	
	return true;
}

void server::acknowledge_all(void)
{
	for( map<ipaddr_t,ipaddr_state_t>::iterator it = ipaddrs_.begin();
		it != ipaddrs_.end(); it++)
	{
		it->second.acknowledge();
	}

	check_state_notify();
}

void server::unacknowledge_all(void)
{
	for( map<ipaddr_t,ipaddr_state_t>::iterator it = ipaddrs_.begin();
		it != ipaddrs_.end(); it++)
	{
		it->second.unacknowledge();
	}

	check_state_notify();
}

void server::check_state_notify(void)
{
	/*TODO: sync */
	
	/* Отсылаем всем окнам сообщение об необходимости проверки
		состояния. Через сообщения, потому что можем находиться
		не в том потоке */
	BOOST_FOREACH(window &win, windows_)
		PostMessage( win.hwnd(), MY_WM_CHECK_STATE, 0, 0);
}

void server::get(my::http::reply &reply, const wstring &request)
{
	tcp::socket socket(io_service_);
	socket.connect(server_endpoint_);
	
	string full_request = "GET "
		+ my::http::percent_encode(my::utf8::encode(request))
		+ " HTTP/1.1\r\n\r\n";

	reply.get(socket, full_request);
}

void server::get_header(tcp::socket &socket, my::http::reply &reply,
	const std::wstring &request)
{
	socket.connect(server_endpoint_);
	
	string full_request = "GET "
		+ my::http::percent_encode(my::utf8::encode(request))
		+ " HTTP/1.1\r\n\r\n";

	reply.get(socket, full_request, false);
}

unsigned int server::load_file(const wstring &file,
	const wstring &file_local, bool throw_if_fail)
{
	my::http::reply reply;

	MY_STOPWATCH_START(__load_file_sw1)
    get(reply, file);
	MY_STOPWATCH_FINISH(__load_file_sw1)

	if (reply.status_code == 200)
	{
		MY_STOPWATCH_START(__load_file_sw2)
		reply.save(file_local);
		MY_STOPWATCH_FINISH(__load_file_sw2)
	
		MY_STOPWATCH_OUT(main_log, L"load_file " << file)
		MY_STOPWATCH_OUT(main_log, L"\nload " << __load_file_sw1)
		MY_STOPWATCH_OUT(main_log, L"\nsave " << __load_file_sw2 << main_log)
	}
	else if (throw_if_fail)
		throw my::exception(L"Запрашиваемый файл не найден")
			<< my::param(L"request", file);
	
	return reply.status_code;
}

void server::paint_tile(Gdiplus::Graphics *canvas,
	int canvas_x, int canvas_y, int z, int x, int y, int level)
{
	int mask = 0;
	int l = level;
	while (l--) mask = (mask << 1) | 1;

	if (z)
	{
		tiler::tile::ptr tile = get_tile(z, x >> level, y >> level);

		if (!tile || !tile->loaded)
			paint_tile(canvas, canvas_x, canvas_y, z - 1, x, y, level + 1);
		else
		{
			int new_w = 256 >> level;

			Gdiplus::Rect rect(canvas_x, canvas_y, 256, 256);
			canvas->DrawImage(&tile->image, rect,
				(x & mask) * new_w, (y & mask) * new_w, new_w, new_w,
				Gdiplus::UnitPixel, NULL, NULL, NULL );
		}
					
		/* Рамка вокруг тайла, если родного нет */
		/*-
		if (level) {
			Gdiplus::Pen pen(Gdiplus::Color(160, 160, 160), 1);
			canvas->DrawRectangle(&pen, canvas_x, canvas_y, 255, 255);
		}
		-*/
	}
}

void server::on_tiler_update()
{
	BOOST_FOREACH(window &win, windows_)
		PostMessage(win.hwnd(), MY_WM_UPDATE, 0, 0);
}

}
