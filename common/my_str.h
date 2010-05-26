#ifndef MY_STR_H
#define MY_STR_H

#include <string>

namespace my { namespace str {


std::string to_string(const wchar_t *str, int len = -1);
inline std::string to_string(const std::wstring &str)
	{ return my::str::to_string(str.c_str(), (int)str.size()); }

std::wstring to_wstring(const char *str, int len = -1);
inline std::wstring to_wstring(const std::string &str)
	{ return my::str::to_wstring(str.c_str(), (int)str.size()); }


enum
{
	escape_double_quotes = 1,
	escape_single_quotes = 2,
	escape_all_quotes = 3,
	quote_by_double_quotes = 4,
	quote_by_single_quotes = 8,
	quote_only_if_need = 16,
	escape_default = 21,
	escape_cntrl_only = 0
};

std::string escape(const char *str, int flags = escape_default, int len = -1);
inline std::string escape(const std::string &str, int flags = escape_default)
	{ return my::str::escape(str.c_str(), flags, (int)str.size()); }

std::wstring escape(const wchar_t *str, int flags = escape_default, int len = -1);
inline std::wstring escape(const std::wstring &str, int flags = escape_default)
	{ return my::str::escape(str.c_str(), flags, (int)str.size()); }

std::string to_hex(const char *str, int len = -1);
inline std::string to_hex(const std::string &str)
	{ return my::str::to_hex(str.c_str(), (int)str.size()); }

std::string from_hex(const char *str, int len = -1);
inline std::string from_hex(const std::string &str)
	{ return my::str::from_hex(str.c_str(), (int)str.size()); }

} }

#endif
