#include <boost/config/warning_disable.hpp> /* против unsafe */

#include "../common/my_http.h"
#include "../common/my_utf8.h"
#include "../common/my_str.h"
#include "../common/my_qi.h"
#include "../common/my_fs.h"
#include "../common/my_exception.h"

#include "../common/my_log.h"
#include "../common/my_time.h"
extern my::log main_log;

#include <errno.h>
#include <string.h> /* strlen */
#include <wchar.h> /* wcslen */

#include <iterator>
using namespace std;

#include <boost/regex.hpp>

namespace my { namespace http {

void parse_request(const string &line,
	string &url, vector< pair<string, string> > &params)
{
	qi::rule<string::const_iterator, string()> url_r
		= +(qi::char_ - ' ' - '?' - '&');
	qi::rule<string::const_iterator, string()> key_r
		= +(qi::char_ - ' ' - '&' - '=');
	qi::rule<string::const_iterator, string()> value_r
		= +(qi::char_ - ' ' - '&');
	qi::rule<string::const_iterator, pair<string, string>()> pair_r
		= key_r >> -('=' >> value_r);
	qi::rule<string::const_iterator, vector< pair<string, string> >()> params_r
		= qi::lit('?') >> pair_r >> *(qi::lit('&') >> pair_r);

	string::const_iterator iter = line.begin();

	bool res = qi::parse( iter, line.end(),
		qi::lit("GET ")
		>> url_r[phoenix::ref(url) = qi::_1]
		>> -params_r[phoenix::ref(params) = qi::_1]
		>> " HTTP/1.1\r\n"
		>> qi::eoi);

	if (!res)
		throw my::exception(L"Не удалось разобрать HTTP-запрос")
			<< my::param(L"http-request", my::str::to_wstring(line))
			<< my::param(L"position", iter - line.begin());
}

unsigned int parse_reply(const string &line,
	string &status_message)
{
	unsigned int status_code;

	string::const_iterator iter = line.begin();

	bool res = qi::parse( iter, line.end(),
		qi::lit("HTTP/1.1 ")
		>> qi::uint_
		>> ' '
		>> +(qi::char_ - '\r' - '\n')
		>> "\r\n"
		>> qi::eoi,
		status_code, status_message);

	if (!res)
		throw my::exception(L"Не удалось разобрать HTTP-ответ")
			<< my::param(L"http-reply", my::str::to_wstring(line))
			<< my::param(L"position", iter - line.begin());

	return status_code;
}

void parse_header(const string &lines,
	vector< pair<string, string> > &params)
{
	qi::rule<string::const_iterator, string()> key_r
		= +(qi::char_ - ' ' - ':' - '\r' - '\n');
	qi::rule<string::const_iterator, string()> value_r
		= *(qi::char_ - '\r' - '\n');
	qi::rule<string::const_iterator, pair<string, string>()> pair_r
		= key_r >> ": " >> value_r >> "\r\n";
	qi::rule<string::const_iterator, vector< pair<string, string> >()> params_r
		= *pair_r;

	string::const_iterator iter = lines.begin();

	bool res = qi::parse( iter, lines.end(),
		params_r >> "\r\n" >> qi::eoi,
		params);

	if (!res)
	{
		/* Считаем, в какой строке ошибка */
		int line = 1;
		int pos = iter - lines.begin();
		int bytes_in_line = 0;

		for (string::const_iterator it = lines.begin();
			it != iter; it++)
		{
			if (*it == '\n')
			{
				pos -= bytes_in_line;
				line++;
				bytes_in_line = 0;
			}
		}

		throw my::exception(L"Не удалось разобрать HTTP-заголовок")
			<< my::param(L"http-header", my::str::to_wstring(lines))
			<< my::param(L"line", line)
			<< my::param(L"position", pos);
	}
}

string percent_decode(const char *str, int len)
{
	string out;

	/* Для нормального преобразования символов с кодами выше 127
		необходимо использовать unsigned char, поэтому оформляем
		это дело отдельным правилом */
	qi::rule<const char *, unsigned char()> char_r
		= qi::char_ - '%'
			| '%' >> qi::uint_parser<unsigned char, 16, 2, 2>();

	if (len < 0)
		len = strlen(str);

	qi::parse(str, str + len,
		*char_r >> qi::eoi, out); /* eoi - указатель конца строки */

	return out;
}

string percent_encode(const char *str,
	const char *escape_symbols, int len)
{
	static const char hex[16] =
	{
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	};

	if (len < 0)
		len = strlen(str);

	const char *ptr_in = str;
	/* Строка может содержать нулевой символ, поэтому
		пользуемся размером исходной строки */
	const char *end_in = ptr_in + len;

	/* Сразу готовим буфер */
	string out(len * 3, ' ');
	char *begin_out = (char*)out.c_str();
	char *ptr_out = begin_out;

	while (ptr_in != end_in)
	{
		char ch = *ptr_in++;
        /* Кодируются все специальные символы, все не ascii-символы (>127),
        	пробел и заказанные пользователем */
		if (ch <= 32 || ch > 127 ||
			escape_symbols && strchr(escape_symbols, ch))
		{
			*ptr_out++ = '%';
			*ptr_out++ = hex[ ((unsigned char)ch) >> 4];
			*ptr_out++ = hex[ ch & 0xf ];
		}
		else
			*ptr_out++ = ch;
	}

	out.resize(ptr_out - begin_out);

	return out;
}


void message::read_header(tcp::socket &socket)
{
	static int count = 0;
	static posix_time::time_duration total_time;
	static posix_time::time_duration min_time(posix_time::hours(24));
	static posix_time::time_duration max_time;

	posix_time::ptime start = posix_time::microsec_clock::local_time();

	size_t n = asio::read_until(socket, buf_, boost::regex("^\r\n"));
	
	posix_time::time_duration time = posix_time::microsec_clock::local_time() - start;
	count++;
	total_time += time;
	if (time < min_time || count == 1)
		min_time = time;
	if (time > max_time)
		max_time = time;

	{
		wstringstream out;
		out << L"read_header\n"
			<< L"count=" << count
			<< L" min=" << min_time
			<< L" avg=" << (total_time / count)
			<< L" max=" << max_time;
		main_log(out.str());
	}

	header_.resize(n);
	buf_.sgetn((char*)header_.c_str(), n);

	vector< pair<string, string> > header_s;
	parse_header(header_, header_s);

	for (vector< pair<string, string> >::iterator iter = header_s.begin();
		iter != header_s.end(); iter++)
	{
		header[ my::utf8::decode( percent_decode(iter->first) ) ]
			= my::utf8::decode( percent_decode(iter->second) );
	}
}

void message::read_body(tcp::socket &socket)
{
	static int count = 0;
	static posix_time::time_duration total_time;
	static posix_time::time_duration min_time(posix_time::hours(24));
	static posix_time::time_duration max_time;

	posix_time::ptime start = posix_time::microsec_clock::local_time();

	/* Чтение здесь всегда заканчивается ошибкой! */
	boost::system::error_code ec;
	asio::read(socket, buf_, boost::asio::transfer_all(), ec);
	
	posix_time::time_duration time = posix_time::microsec_clock::local_time() - start;
	count++;
	total_time += time;
	if (time < min_time || count == 1)
		min_time = time;
	if (time > max_time)
		max_time = time;

	{
		wstringstream out;
		out << L"read_body\n"
			<< L"count=" << count
			<< L" min=" << min_time
			<< L" avg=" << (total_time / count)
			<< L" max=" << max_time;
		main_log(out.str());
	}

	size_t n = buf_.size();
	body.resize(n);
	buf_.sgetn((char*)body.c_str(), n);
}

wstring message::content_type()
{
	wstring value = header[L"Content-Type"];
	
	value = value.substr(0, value.find_first_of(L';'));

	if (value.empty())
		value = L"text/plain"; /* Так требует RFC! */
	
	return value;
}

void message::to_xml(::xml::ptree &pt)
{
	stringstream ss(body);
	my::xml::parse(ss, pt);
}

void message::to_xml(::xml::wptree &pt)
{
	wstringstream ss( my::utf8::decode(body) );
	my::xml::parse(ss, pt);
}

void message::save(const wstring &filename)
{
	fs::create_directories( fs::wpath(filename).parent_path() );

	ofstream fs(filename.c_str(), ios::binary);
	if (fs)
		fs << body;
	
	if (!fs)
		throw my::exception(L"Не удалось сохранить данные в файл")
			<< my::param(L"file", filename)
			<< my::param(L"error", strerror(errno));
}

void reply::read_reply(tcp::socket &socket)
{
	static int count = 0;
	static posix_time::time_duration total_time;
	static posix_time::time_duration min_time(posix_time::hours(24));
	static posix_time::time_duration max_time;

	posix_time::ptime start = posix_time::microsec_clock::local_time();

	size_t n = asio::read_until(socket, buf_, "\r\n");
	posix_time::time_duration time = posix_time::microsec_clock::local_time() - start;
	count++;
	total_time += time;
	if (time < min_time || count == 1)
		min_time = time;
	if (time > max_time)
		max_time = time;

	{
		wstringstream out;
		out << L"read_reply\n"
			<< L"count=" << count
			<< L" min=" << min_time
			<< L" avg=" << (total_time / count)
			<< L" max=" << max_time;
		main_log(out.str());
	}

	reply_.resize(n);
	buf_.sgetn((char*)reply_.c_str(), n);

	string status_message_s;
	status_code = parse_reply(reply_, status_message_s);

	status_message = my::utf8::decode( percent_decode(status_message_s) );
}

void reply::get(tcp::socket &socket,
	const string &request, bool do_read_body)
{
	static int count = 0;
	static posix_time::time_duration total_time;
	static posix_time::time_duration min_time(posix_time::hours(24));
	static posix_time::time_duration max_time;

	posix_time::ptime start = posix_time::microsec_clock::local_time();

	asio::write(socket, asio::buffer(request), asio::transfer_all());
	
	read_reply(socket);
	read_header(socket);
	
	if (do_read_body)
		read_body(socket);

	posix_time::time_duration time = posix_time::microsec_clock::local_time() - start;

	count++;

	total_time += time;
	if (time < min_time || count == 1)
		min_time = time;
	if (time > max_time)
		max_time = time;

	//if ((count & 0x0F) == 0)
	{
		wstringstream out;
		out << L"get\n"
			<< L"count=" << count
			<< L" min=" << min_time
			<< L" avg=" << (total_time / count)
			<< L" max=" << max_time;
		main_log(out.str());
	}
}

void request::read_request(tcp::socket &socket)
{
	size_t n = asio::read_until(socket, buf_, "\r\n");

	request_.resize(n);
	buf_.sgetn((char*)request_.c_str(), n);

	string url_s;
	vector< pair<string, string> > params_s;

	parse_request(request_, url_s, params_s);

	url = my::utf8::decode( percent_decode(url_s) );

	for (vector< pair<string, string> >::iterator iter = params_s.begin();
		iter != params_s.end(); iter++)
	{
		params[ my::utf8::decode( percent_decode(iter->first) ) ]
			= my::utf8::decode( percent_decode(iter->second) );
	}
}

} }
