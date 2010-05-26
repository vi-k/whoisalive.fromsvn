#ifndef MY_TIME_H
#define MY_TIME_H

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
namespace posix_time=boost::posix_time;

#include "boost/date_time/local_time/local_time.hpp"
namespace local_time=boost::local_time;

namespace my { namespace time
{

posix_time::ptime utc_to_local(const posix_time::ptime &utc_time);
posix_time::ptime local_to_utc(const posix_time::ptime &local_time);

std::wstring to_fmt_wstring(const wchar_t *fmt,
	const posix_time::ptime &time);
std::wstring to_fmt_wstring(const wchar_t *fmt,
	const posix_time::time_duration &time);

inline std::wstring to_wstring(const posix_time::ptime &time)
	{ return my::time::to_fmt_wstring(L"%Y-%m-%d %H:%M:%S%F", time); }
inline std::wstring to_wstring(const posix_time::time_duration &time)
	{ return to_fmt_wstring(L"%-%H:%M:%S%F", time); }

void throw_if_fail(const posix_time::ptime &time);
void throw_if_fail(const posix_time::time_duration &time);

posix_time::time_duration to_duration(const std::string &str);
posix_time::time_duration to_duration(const std::wstring &str);

posix_time::ptime to_time_fmt(const wchar_t *str, const wchar_t *fmt);
inline posix_time::ptime to_time_fmt(const std::wstring &str, const wchar_t *fmt)
	{ return to_time_fmt(str.c_str(), fmt); }
inline posix_time::ptime to_time(const wchar_t *str)
	{ return to_time_fmt(str, L"%Y-%m-%d %H:%M:%S%F"); }
inline posix_time::ptime to_time(const std::wstring &str)
	{ return to_time_fmt(str.c_str(), L"%Y-%m-%d %H:%M:%S%F"); }

} }

#endif
