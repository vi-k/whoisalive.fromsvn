#include "my_inet.h"
#include "my_str.h"
#include "my_punycode.h"

using namespace std;

namespace my { namespace ip {

string to_string(const ::ip::address_v4 &address)
{
	return address.to_string();
}

wstring to_wstring(const ::ip::address_v4 &address)
{
	return my::str::to_wstring( address.to_string() );
}

string punycode_encode(const wchar_t *str, int len)
{
	string out;
	const wchar_t *ptr = str;
	const wchar_t *end;

	if (len < 0)
		len = wcslen(str);
	
	end = ptr + len;

	/* Конвертируем каждую часть имени хоста отдельно */
	while (true)
	{
		while (*ptr != L'.' && ptr != end)
			ptr++;

		if (ptr == end)
			break;

		out += my::punycode_encode(str, ptr - str);
		out += '.';

		ptr++; /* Пропускаем точку */
		str = ptr; /* Идём к следующей части */
	}

	out += my::punycode_encode(str, ptr - str);

	return out;
}

wstring punycode_decode(const char *str, int len)
{
	wstring out;
	const char *ptr = str;
	const char *end;

	if (len < 0)
		len = strlen(str);
	
	end = ptr + len;

	/* Конвертируем каждую часть имени хоста отдельно */
	while (true)
	{
		while (*ptr != '.' && ptr != end)
			ptr++;

		if (ptr == end)
			break;

		out += my::punycode_decode(str, ptr - str);
		out += L'.';

		ptr++; /* Пропускаем точку */
		str = ptr; /* Идём к следующей части */
	}

	out += my::punycode_decode(str, ptr - str);

	return out;
}

} }
