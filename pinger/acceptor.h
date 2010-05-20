#ifndef SERVER_H
#define SERVER_H

#include "pinger.h"
#include "connection.h"
#include "eventer.h"

#include "../common/my_xml.h"
#include "../common/my_inet.h"
#include "../common/my_thread.h"

#include <memory>
#include <map>

#include <boost/utility.hpp>

namespace acceptor {

struct map_info
{
	std::wstring tile_type;
	std::wstring ext;
};
typedef std::map<std::wstring, map_info> map_info_map;

class server : private boost::noncopyable
{
	private:
		asio::io_service io_service_;
		ip::tcp::acceptor acceptor_;
		std::auto_ptr<connection> new_connection_;
		boost::thread_group thgroup_;
		pinger::server pinger_;
		eventer::server state_eventer_;
		eventer::server ping_eventer_;
		map_info_map maps_;

		void handle_accept(const boost::system::error_code &e);
		void accept(void);
    
	public:
		server(xml::wptree &config);

		void run();

		void on_change_state(const pinger::host_pinger_copy &pinger);
		void on_ping(const pinger::host_pinger_copy &pinger,
			const pinger::ping_result &result);

		inline asio::io_service& io_service() { return io_service_; }
		inline pinger::server& pinger() { return pinger_; }
		inline eventer::server& state_eventer() { return state_eventer_; }
		inline eventer::server& ping_eventer() { return ping_eventer_; }
		inline map_info_map& maps() { return maps_; }
};

}

#endif
