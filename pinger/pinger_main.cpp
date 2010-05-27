#include "config.h"

#include "acceptor.h"

#include "../common/my_time.h"
#include "../common/my_xml.h"
#include "../common/my_fs.h"
#include "../common/my_exception.h"
#include "../common/my_log.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <locale>
#include <memory>
using namespace std;

#include <boost/archive/detail/utf8_codecvt_facet.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string/replace.hpp>

BOOL __stdcall CtrlHandlerRoutine(DWORD dwCtrlType);

wofstream main_log_stream;
void on_main_log(const wstring &text)
{
	main_log_stream << L"[" << my::time::to_fmt_wstring(L"%Y-%m-%d %H:%M:%S",
		posix_time::microsec_clock::universal_time()) << L"]\n";
	main_log_stream << text << endl << endl;
	main_log_stream.flush();
}
my::log main_log(on_main_log);

int main()
{
	try
	{
		setlocale(LC_ALL, ""); /* для wcstombs и подобных */

		bool log_exists = fs::exists(L"pinger.log");

		main_log_stream.open(L"pinger.log", ios::app);

		if (!log_exists)
			main_log_stream << L"\xEF\xBB\xBF";
		else
			main_log_stream << endl;
		
		main_log_stream.imbue( locale( main_log_stream.getloc(),
			new boost::archive::detail::utf8_codecvt_facet) );

		#ifdef _DEBUG
		wcout << L"Debug: " << VERSION << L" " << BUILDNO
			<< L" " << BUILDDATE L" " << BUILDTIME << endl;
		main_log << L"Start\n"
			<< L"Debug: " VERSION L" " BUILDNO L" " BUILDDATE L" " BUILDTIME
			<< main_log;
		#else
		wcout << L"Release: " << VERSION << endl;
		main_log << L"Start\n"
			<< L"Release: " VERSION
			<< main_log;
		#endif


		/* Реакция на нажатие Ctrl-C */
		SetConsoleCtrlHandler(CtrlHandlerRoutine, TRUE);

		auto_ptr<acceptor::server> acceptor;

		try
		{
			xml::wptree pt;
			my::xml::load(L"config.xml", pt);
			acceptor.reset( new acceptor::server(pt.get_child(L"config")) );
		}
		catch(my::exception &e)
		{
			throw my::exception(L"Ошибка инициализации сервера")
				<< my::param(L"config", L"config.xml")
				<< e;
		}
		catch(exception &e)
		{
			throw my::exception(L"Ошибка инициализации сервера")
				<< my::param(L"config", L"config.xml")
				<< e;
		}
		
		acceptor->run();
	}
	catch (my::exception &e)
	{
		wstring error = e.message();
		wcout << L"\n-- my::exception --\n"
			<< error << endl;
		main_log << L"-- my::exception --\n"
			<< error << main_log;
    }
	catch (std::exception &e)
	{
		my::exception my_e(e);
		wstring error = my_e.message();
		wcout << L"\n-- std::exception --\n"
			<< error << endl;
		main_log << L"-- std::exception --\n"
			<< error << main_log;
    }

    return 0;
}

/* Обработчик нажатий Ctrl-Break */
BOOL __stdcall CtrlHandlerRoutine(DWORD dwCtrlType)
{
	main_log << L"Break" << main_log;
	return FALSE;
}
