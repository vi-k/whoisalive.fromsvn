#ifndef EVENTER_H
#define EVENTER_H

#include "connection.h"

#include "../common/my_thread.h"
#include "../common/my_inet.h"

#include <list>
#include <boost/ptr_container/ptr_list.hpp>
#include <boost/function.hpp>

namespace eventer {

/* Класс - событие */
struct event
{
	ip::address_v4 address; /* С какого адреса событие */
	std::wstring message; /* Что за событие */

	event(ip::address_v4 who, const std::wstring &what)
		: address(who), message(what) {}
};

/* Класс - удалённый обработчик событий */
struct handler
{
	ip::address_v4 address;
	acceptor::connection *connection;

	handler(acceptor::connection *con, ip::address_v4 event_address)
		: connection(con)
		, address(event_address)
	{
	}

	~handler()
	{
		delete connection;
	}
};

class server
{
	private:
		boost::ptr_list<handler> handlers_;
		std::list<event> events_;
		mutex handlers_mutex_; /* Блокировка списка обработчиков */
		mutex events_mutex_; /* Блокировка списка событий */
		condition_variable cond_;

	public:
		server();
		~server()
		{
		}

		void run();
		void start();

		scoped_lock get_lock()
			{ return scoped_lock(handlers_mutex_); }

		void add_handler(acceptor::connection *connection,
			ip::address_v4 for_whom);

		void add_event(ip::address_v4 who, const std::wstring &what);

		static bool handler_failed(const handler &_handler)
			{ return !_handler.connection->socket().is_open(); }
};

}

#endif
