#include "my_xml.h"
#include "my_utf8.h"
#include "my_str.h"
#include "my_exception.h"

#include <locale>
#include <sstream>
#include <fstream>
using namespace std;

#include <boost/optional.hpp>
#include <boost/foreach.hpp>

#include <boost/archive/codecvt_null.hpp>
#include <libs/serialization/src/codecvt_null.cpp>

wstring xmlattr_to_str(const xml::wptree::value_type &v)
{
	wstringstream s;

	s << L"<" << v.first;

	boost::optional<const xml::wptree&> opt
		= v.second.get_child_optional(L"<xmlattr>");

	if (opt)
	{
		BOOST_FOREACH(const xml::wptree::value_type &v, *opt)
			/*TODO: не учитываю необходимость преобразования кавычек */
			s << L" " << v.first << L"=\"" << v.second.data() << L"\"";
	}

	if (v.second.size() == 0)
		s << L" />";
	else
		s << L"...</" << v.first << ">";
	
	return s.str();
}

wstring xmlattr_to_str(const xml::wptree &pt)
{
	wstringstream s;
	bool first = true;

	boost::optional<const xml::wptree&> opt
		= pt.get_child_optional(L"<xmlattr>");

	if (opt)
	{
		BOOST_FOREACH(const xml::wptree::value_type &v, *opt)
		{
			if (first)
				first = false;
			else
				s << L" ";

			/*TODO: не учитываю необходимость преобразования кавычек */
			s << v.first << L"=\"" << v.second.data() << L"\"";
		}
	}

	if ( pt.size() > (size_t)(opt ? 1 : 0) )
		s << L" ...";
	
	return s.str();
}

void my::xml::load(const std::wstring &filename, ::xml::wptree &pt)
{
	wifstream fs( filename.c_str() );

	if (!fs)
		throw my::exception(L"Не удалось открыть файл")
			<< my::param(L"file", filename);

	/* Загрузка BOM */
	wchar_t ch = fs.get();
		
	/* utf8 */
	if (ch == 0x00EF && fs.get() == 0x00BB && fs.get() == 0xBF)
		fs.imbue( locale( fs.getloc(),
			new boost::archive::detail::utf8_codecvt_facet) );
	/* unicode */
	else if (ch == 0x00FF && fs.get() == 0x00FE)
		fs.imbue( locale( fs.getloc(),
			new boost::archive::codecvt_null<wchar_t>) );
	/* ansi */
	else
	{
		fs.imbue( locale("") );
		fs.seekg(0);
	}

	try
	{
		my::xml::parse(fs, pt);
	}
	catch(my::exception &e)
	{
		e.message(L"Ошибка разбора xml-файла");
		throw e << my::param(L"file", filename);
	}
}
