#ifndef CONNECTION_H
#define CONNECTION_H

#include "../common/my_inet.h"
#include "../common/my_xml.h"
#include "../common/my_utf8.h"

#include <string>

namespace acceptor {

class server;
class connection
{
private:
	server &server_;
	tcp::socket socket_;

public:
	connection(server &server);
	~connection();

	inline tcp::socket &socket() { return socket_; }
		
	void run();

	void send(const std::string &data)
	{
		asio::write(socket_, asio::buffer(data), asio::transfer_all());
	}

	void send_in_utf8(const std::wstring &data)
	{
		send( my::utf8::encode(data) );
	}

	void send_header(unsigned int status_code,
		const std::string &status_message,
		const std::string &content_type);
	
	void send_404(const std::wstring &message);

	inline void send_ok(const std::string &content_type)
	{
		send_header(200, "OK", content_type);
	}

	bool send_file(const std::wstring &filename,
		const std::string &content_type);
	
	void send_xml(xml::wptree &pt);
};

}

#endif
