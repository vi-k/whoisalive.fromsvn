#ifndef MY_TIME_H
#define MY_TIME_H

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
namespace posix_time=boost::posix_time;

namespace my { namespace time
{

std::wstring to_fmt_wstring(const wchar_t *fmt,
	const posix_time::ptime &time);
std::wstring to_fmt_wstring(const wchar_t *fmt,
	const posix_time::time_duration &time);

std::wstring to_wstring(const posix_time::ptime &time);
std::wstring to_wstring(const posix_time::time_duration &time);

void throw_if_fail(const posix_time::ptime &time);
void throw_if_fail(const posix_time::time_duration &time);

posix_time::time_duration to_duration(const std::string &str);
posix_time::time_duration to_duration(const std::wstring &str);

} }

#endif
