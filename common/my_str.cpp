#include <boost/config/warning_disable.hpp> /* против unsafe */

#include "my_str.h"

#include <stdlib.h>
#include <string.h> /* strlen */
#include <wchar.h> /* wcslen */

#include <sstream>
#include <locale>
using namespace std;

namespace my { namespace str {

string to_string(const wchar_t *str, int len)
{
	if (len < 0)
		len = wcslen(str);
	string out(len, ' ');
	wcstombs((char*)out.c_str(), str, len);
	return out;
}

wstring to_wstring(const char *str, int len)
{
	if (len < 0)
		len = strlen(str);
	wstring out(len, L' ');
	mbstowcs((wchar_t*)out.c_str(), str, len);
	return out;
}

string escape(const char *ptr, int flags, int len)
{
	const char *esc[] = {
		"\\0",   "\\x01", "\\x02", "\\x03",
		"\\x04", "\\x05", "\\x06", "\\a",
		"\\b",   "\\t",   "\\n",   "\\v",
		"\\f",   "\\r",   "\\x0e", "\\x0f",
		"\\x10", "\\x11", "\\x12", "\\x13",
		"\\x14", "\\x15", "\\x16", "\\x17",
		"\\x18", "\\x19", "\\x1a", "\\x1b",
		"\\x1c", "\\x1d", "\\x1e", "\\x1f"};

	if (len < 0)
		len = strlen(ptr);

	const char *end = ptr + len; 

	/* Сразу готовим буфер */
	string out(len * 4, ' ');
	char *begin_out = (char*)out.c_str();
	char *ptr_out = begin_out;

	bool need_for_quote = false;

	while (ptr != end)
	{
		char ch = *ptr++;
		if (ch >= 0 && ch < 32)
		{
			const char *val = esc[ch];
			while (*val)
				*ptr_out++ = *val++;
			need_for_quote = true;
		}
		else
		{
			if (ch == '\"' && (flags & escape_double_quotes)
				|| ch == '\'' && (flags & escape_single_quotes)
				|| ch == '\\')
			{
				*ptr_out++ = '\\';
				need_for_quote = true;
			}

			*ptr_out++ = ch;
		}
	}

	out.resize(ptr_out - begin_out);

	if ( flags & quote_by_double_quotes &&
		!(flags & quote_only_if_need && !need_for_quote) )
		out = '\"' + out + '\"';
	else if ( flags & quote_by_single_quotes &&
		!(flags & quote_only_if_need && !need_for_quote) )
		out = '\'' + out + '\'';

	return out;
}

wstring escape(const wchar_t *ptr, int flags, int len)
{
	const wchar_t *esc[] = {
		L"\\0",   L"\\x01", L"\\x02", L"\\x03",
		L"\\x04", L"\\x05", L"\\x06", L"\\a",
		L"\\b",   L"\\t",   L"\\n",   L"\\v",
		L"\\f",   L"\\r",   L"\\x0e", L"\\x0f",
		L"\\x10", L"\\x11", L"\\x12", L"\\x13",
		L"\\x14", L"\\x15", L"\\x16", L"\\x17",
		L"\\x18", L"\\x19", L"\\x1a", L"\\x1b",
		L"\\x1c", L"\\x1d", L"\\x1e", L"\\x1f"};

	if (len < 0)
		len = wcslen(ptr);

	const wchar_t *end = ptr + len; 

	/* Сразу готовим буфер */
	wstring out(len * 4, L' ');
	wchar_t *begin_out = (wchar_t*)out.c_str();
	wchar_t *ptr_out = begin_out;

	bool need_for_quote = false;

	while (ptr != end)
	{
		wchar_t ch = *ptr++;
		if (ch >= 0 && ch < 32)
		{
			const wchar_t *val = esc[ch];
			while (*val)
				*ptr_out++ = *val++;
			need_for_quote = true;
		}
		else
		{
			if (ch == L'\"' && (flags & escape_double_quotes)
				|| ch == L'\'' && (flags & escape_single_quotes)
				|| ch == L'\\')
			{
				*ptr_out++ = L'\\';
				need_for_quote = true;
			}

			*ptr_out++ = ch;
		}
	}

	out.resize(ptr_out - begin_out);

	if ( flags & quote_by_double_quotes &&
		!(flags & quote_only_if_need && !need_for_quote) )
		out = L'\"' + out + L'\"';
	else if ( flags & quote_by_single_quotes &&
		!(flags & quote_only_if_need && !need_for_quote) )
		out = L'\'' + out + L'\'';

	return out;
}

string to_hex(const char *str, int len)
{
	static const char hex[16] =
	{
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	};

	if (len < 0)
		len = strlen(str);

	const char *ptr_in = str;
	const char *end_in = ptr_in + len;

	/* Сразу готовим буфер */
	string out(len * 2, ' ');
	char *ptr_out = (char*)out.c_str();

	while (ptr_in != end_in)
	{
		char ch = *ptr_in++;
		*ptr_out++ = hex[ ((unsigned char)ch) >> 4];
		*ptr_out++ = hex[ ch & 0xf ];
	}

	return out;
}

string from_hex(const char *str, int len)
{
	if (len < 0)
		len = strlen(str);

	const char *ptr_in = str;
	const char *end_in = ptr_in + len;

	string out( (len + 1) >> 1, ' ' );
	char *begin_out = (char*)out.c_str();
	char *ptr_out = begin_out;

	bool even = false;

	while (ptr_in != end_in)
	{
		char ch = *ptr_in++;
		if (ch >= 48 && ch <= 57) /* 0..9 */
			ch -= 48;
		else if (ch >= 65 && ch <= 70) /* A..F */
			ch -= 55;
		else if (ch >= 97 && ch <= 102) /* a..f */
			ch -= 87;
		else
			break;

		if (!even)
			*ptr_out = ch << 4;
		else
			*ptr_out++ = *ptr_out | ch;

		even = !even;
	}

	out.resize(ptr_out - begin_out);

	/* Пользователь может проверить результат:
		ошибка, если out.size() * 2 != len */

	return out;
}

} }
