#ifndef MY_XML_H
#define MY_XML_H

#include "my_exception.h"

#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/detail/xml_parser_writer_settings.hpp>

namespace xml=boost::property_tree;


namespace my { namespace xml {

/* Загрузка xml-файла. Автоматическое
	преобразование ansi/utf8/unicode */
void load(const std::wstring &filename, ::xml::wptree &pt);

/* Загрузка xml из потока с отловом и преобразованием исключений */
template<class Stream, class Ptree>
void parse(Stream &os, Ptree &pt)
{
	try
	{
		::xml::read_xml(os, pt);
	}
	catch(::xml::xml_parser_error &e)
	{
		throw my::exception(L"Ошибка разбора xml-данных")
			<< my::param(L"line", e.line())
			<< my::param(L"error", my::str::to_wstring(e.message()));
	}
}

} }

/* Преобразование xmlattr в строку (depricated) */
std::wstring xmlattr_to_str(const xml::wptree::value_type &v);
std::wstring xmlattr_to_str(const xml::wptree &pt);

#endif
