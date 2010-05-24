#include "my_time.h"
#include "my_exception.h"

#include <sstream>
#include <locale> /* facet */
using namespace std;

#include <boost/date_time/time_parsing.hpp>
#include "boost/date_time/c_local_time_adjustor.hpp"

namespace my { namespace time {

posix_time::ptime utc_to_local(const posix_time::ptime &utc_time)
{
	return boost::date_time::c_local_adjustor<posix_time::ptime>
		::utc_to_local(utc_time);
}

posix_time::ptime local_to_utc(const posix_time::ptime &local_time)
{
	/* Прямой функции для c_local_adjustor почему-то нет */
	posix_time::time_duration td = utc_to_local(local_time) - local_time;
	return local_time - td;
}

wstring to_fmt_wstring(const wchar_t *fmt, const posix_time::ptime &time)
{
	wstringstream out;

	out.imbue( locale(out.getloc(), new posix_time::wtime_facet(fmt)) );
	out << time;
	
	return out.str();
}

wstring to_fmt_wstring(const wchar_t *fmt, const posix_time::time_duration &time)
{
	wstringstream out;

	out.imbue( locale(out.getloc(), new posix_time::wtime_facet(fmt)) );
	out << time;
	
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

posix_time::ptime to_time_fmt(const wchar_t *str, const wchar_t *fmt)
{
	wstringstream in(str);
	posix_time::ptime time;

	in.imbue( locale(in.getloc(),
		new posix_time::wtime_input_facet(fmt)) );
	in >> time;

	return time;
}

} }
