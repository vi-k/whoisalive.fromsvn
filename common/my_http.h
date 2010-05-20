#ifndef MY_HTTP_H
#define MY_HTTP_H

#include "my_inet.h"
#include "my_xml.h"

#include <string>
#include <map>
#include <vector>
#include <utility> /* std::pair */

namespace my { namespace http {

/* Разбор строки http-запроса в виде:
	GET /path/to/file?param1=value1&param2=value2&param3=value3... HTTP/1.1\r\n
	Выделение из него, собственно, пути и параметров */
void parse_request(const std::string &line,
	std::string &url,
	std::vector< std::pair<std::string, std::string> > &params);

/* Разбор строки http-ответа в виде:
	HTTP/1.1 200 OK\r\n
	HTTP/1.1 404 Page Not Found\r\n
	Выделение из него кода и сообщения */
unsigned int parse_reply(const std::string &line,
	std::string &status_message);

/* Разбор заголовка (принцип построение одинаков и у запроса и у ответа)
	в виде:
	Content-Type: text/plain; charset=utf8\r\n
	Connection: close\r\n\r\n */
void parse_header(const std::string &lines,
	std::vector< std::pair<std::string, std::string> > &params);

std::string percent_decode(const char *str, int len = -1);
inline std::string percent_decode(const std::string &str)
{
	return percent_decode(str.c_str(), (int)str.size());
}

std::string percent_encode(const char *str,
	const char *escape_symbols = NULL, int len = -1);
inline std::string percent_encode(const std::string &str,
	const char *escape_symbols = NULL)
{
	return percent_encode(str.c_str(), escape_symbols, (int)str.size());
}

class message
{
public:
	asio::streambuf buf_; /* Буфер для чтения их сокета*/
	std::string header_; /* Заголовок AS IS */
	std::map<std::wstring, std::wstring> header;
	std::string body;

	message() {}

	void read_header(tcp::socket &socket);
	void read_body(tcp::socket &socket);

	std::wstring content_type();

	void to_xml(::xml::ptree &pt);
	void to_xml(::xml::wptree &pt);

	void save(const std::wstring &filename);
};

class reply : public message
{
public:
	std::string reply_; /* Строка ответа AS IS */
	unsigned int status_code;
	std::wstring status_message;

	reply() : message(), status_code(0) {}

	void read_reply(tcp::socket &socket);

	/* Отправка запроса и получение ответа */
	void reply::get(tcp::socket &socket,
		const std::string &request, bool do_read_body = true);
};

class request : public message
{
public:
	std::string request_; /* Строка запроса AS IS */
	std::wstring url; /* URL */
	std::map<std::wstring, std::wstring> params; /* Параметры: ?a=A&b=B... */

	request() : message() {}

	void read_request(tcp::socket &socket);
};

} }

#endif
