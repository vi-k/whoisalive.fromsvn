#include "my_time.h"
#include "my_exception.h"

#include <sstream>
#include <locale> /* facet */
using namespace std;

#include <boost/date_time/time_parsing.hpp>

namespace my { namespace time {

wstring to_fmt_wstring(const wchar_t *fmt, const posix_time::ptime &time)
{
	wstringstream out;

	posix_time::wtime_facet* facet( new posix_time::wtime_facet(fmt) );
	out.imbue( locale(out.getloc(), facet) );
	out << time;
	
	return out.str();
}

wstring to_fmt_wstring(const wchar_t *fmt, const posix_time::time_duration &time)
{
	wstringstream out;

	posix_time::wtime_facet* facet( new posix_time::wtime_facet(fmt) );
	out.imbue( locale(out.getloc(), facet) );
	out << time;
	
	return out.str();
}

wstring to_wstring(const posix_time::ptime &time)
{
	return my::time::to_fmt_wstring(L"%Y-%m-%d %H:%M:%S%F %z", time);
}

wstring to_wstring(const posix_time::time_duration &time)
{
	wstringstream out;
	out << time; // to_fmt_wstring(L"%-%H:%M:%S%F !", time);
	return out.str();
}

void throw_if_fail(const posix_time::ptime &time)
{
	if (time.is_special())
		throw my::exception(L"Время указано неверно")
			<< my::param(L"time", time);
}

void throw_if_fail(const posix_time::time_duration &time)
{
	if (time.is_special())
		throw my::exception(L"Время указано неверно")
			<< my::param(L"time", time);
}

posix_time::time_duration to_duration(const string &str)
{
	return boost::date_time::str_from_delimited_time_duration<
		posix_time::time_duration,char>(str);
}

posix_time::time_duration to_duration(const wstring &str)
{
	return boost::date_time::str_from_delimited_time_duration<
		posix_time::time_duration,wchar_t>(str);
}

} }
