#ifndef MY_INET_H
#define MY_INET_H

#undef _WIN32_WINNT 
#define _WIN32_WINNT 0x0501
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp>

namespace asio=boost::asio;
namespace ip=boost::asio::ip;
using boost::asio::ip::icmp;
using boost::asio::ip::tcp;

namespace my { namespace ip
{

std::string to_string(const ::ip::address_v4 &address);
std::wstring to_wstring(const ::ip::address_v4 &address);

std::string punycode_encode(const wchar_t *str, int len = -1);
inline std::string punycode_encode(const std::wstring &str)
{
	return my::ip::punycode_encode(str.c_str(), (int)str.size());
}

std::wstring punycode_decode(const char *str, int len = -1);
inline std::wstring punycode_decode(const std::string &str)
{
	return my::ip::punycode_decode(str.c_str(), (int)str.size());
}

} }

#endif
