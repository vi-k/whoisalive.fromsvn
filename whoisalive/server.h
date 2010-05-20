#ifndef IPOWNER_H
#define IPOWNER_H

#include "ipgui.h"
#include "ipaddr.h"
#include "obj_class.h"
#include "ipwindow.h"
#include "tiler.h"

#include "../common/my_inet.h"
#include "../common/my_http.h"
#include "../common/my_thread.h"
#include "../common/my_xml.h"
#include "../common/my_ptr.h"

#include <map>
#include <memory>

#include <boost/ptr_container/ptr_list.hpp>
#include <boost/unordered_map.hpp>

namespace who
{

/* Сервер (по отношению к модулям приложения,
	а не в терминах клиент-серверной технологии) */
class server
{
private:
	boost::thread_group thgroup_;
	asio::io_service io_service_;
	tcp::endpoint server_endpoint_;
	tcp::socket state_log_socket_;
	ULONG_PTR gdiplus_token_;
	boost::unordered_map<std::wstring, who::obj_class::ptr> classes_;
	boost::ptr_list<ipwindow_t> windows_;
	int anim_period_;
	int def_anim_steps_;
	std::map<ipaddr_t, ipaddr_state_t> ipaddrs_;
	tiler::server tiler_;
	int active_map_id_;

	void load_classes_(void);
	void load_maps_(void);

	bool add_addr_(const ipaddr_t &ipaddr);

	void state_log_thread_proc_(void);

public:
	server(const xml::wptree &config);
	~server();

	inline int def_anim_steps(void) { return def_anim_steps_; }
	inline int anim_period(void) { return anim_period_; }
		
	inline who::obj_class::ptr
		obj_class(const std::wstring &class_name)
			{ return classes_[class_name]; }

	ipwindow_t* add_window(HWND hwnd);

	void get(my::http::reply &reply, const std::wstring &request);
	unsigned int load_file(const std::wstring &file,
		const std::wstring &file_local, bool throw_if_fail = true);

	/* Регистрация ip-адресов */
	void register_ipaddr(const ipaddr_t &addr);
	void unregister_ipaddr(const ipaddr_t &addr);

	/* Состояние ip-адресов, квитирование */
	ipaddr_state_t ipaddr_state(const ipaddr_t &addr)
		{ return ipaddrs_[addr]; }
	
	void acknowledge(const ipaddr_t &addr)
	{
		ipaddrs_[addr].acknowledge();
		check_state_notify();
	}
	
	void unacknowledge(const ipaddr_t &addr)
	{
		ipaddrs_[addr].unacknowledge();
		check_state_notify();
	}
	
	void acknowledge_all(void);
	void unacknowledge_all(void);

	void check_state_notify(void);

	inline tiler::tile_ptr get_tile(int z, int x, int y)
		{ return tiler_.get_tile(active_map_id_, z, x, y); }

	inline void set_active_map(int id)
		{ active_map_id_ = id; }

	void paint_tile(Gdiplus::Graphics *canvas,
		int canvas_x, int canvas_y, int z, int x, int y, int level = 0);
	void on_tiler_update();
};

}

#endif
