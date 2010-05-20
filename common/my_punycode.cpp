/* 
	Код основан на GNU Libidn (punycode.c):

	punycode.c - Implementation of punycode used to ASCII encode IDN's
	Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010
	Simon Josefsson

	А он в свою очередь:

	This file is derived from RFC 3492bis written by Adam M. Costello.
	Copyright (C) The Internet Society (2003).  All Rights Reserved.
*/

#include "my_punycode.h"
#include "my_exception.h"

using namespace std;

namespace my {

/* punycode_uint needs to be unsigned and needs to be */
/* at least 26 bits wide.                             */
typedef unsigned int punycode_uint;

/*** Bootstring parameters for Punycode ***/
enum
{
	base = 36,
	tmin = 1,
	tmax = 26,
	skew = 38,
	damp = 700,
	initial_bias = 72,
	initial_n = 0x80
};

/* decode_digit(cp) returns the numeric value of a basic code */
/* point (for use in representing integers) in the range 0 to */
/* base-1, or base if cp does not represent a value.          */

punycode_uint decode_digit(punycode_uint cp)
{
	return cp - 48 < 10 ? cp - 22
		: cp - 65 < 26 ? cp - 65
		: cp - 97 < 26 ? cp - 97
		: base;
}

/* encode_digit(d,flag) returns the basic code point whose value      */
/* (when used for representing integers) is d, which needs to be in   */
/* the range 0 to base-1.  The lowercase form is used unless flag is  */
/* nonzero, in which case the uppercase form is used.  The behavior   */
/* is undefined if flag is nonzero and digit d has no uppercase form. */

char encode_digit(punycode_uint d)
{
  return d + 22 + 75 * (d < 26);
  /*  0..25 map to ASCII a..z or A..Z */
  /* 26..35 map to ASCII 0..9         */
}

/*** Platform-specific constants ***/

/* maxint is the maximum value of a punycode_uint variable: */
const punycode_uint maxint = -1;
/* Because maxint is unsigned, -1 becomes the maximum value. */

/*** Bias adaptation function ***/

punycode_uint adapt(punycode_uint delta, punycode_uint numpoints, int firsttime)
{
	punycode_uint k;

	delta = firsttime ? delta / damp : delta >> 1;
	
	delta += delta / numpoints;

	for (k = 0; delta > ((base - tmin) * tmax) / 2; k += base)
		delta /= base - tmin;

	return k + (base - tmin + 1) * delta / (delta + skew);
}

/*** Main encode function ***/

string punycode_encode(const wchar_t *str, size_t len)
{
    string out;
    const wchar_t *end = str + len;

    /* Символы ascii (00..7F) просто копируем */
    for (const wchar_t *ptr = str; ptr != end; ptr++)
		if (*ptr >= 0 && *ptr <= 0x7F)
			out.push_back( (char)*ptr );

	punycode_uint h = out.size(); /* Кол-во обработанных символов */
	punycode_uint b = h; /* Количество скопированных базовых (ascii) символов */

	if (h < len)
	{
		out = "xn--" + out;
		if (b > 0)
			out.push_back('-');
	}

	punycode_uint n = initial_n;
	punycode_uint delta = 0;
	punycode_uint bias = initial_bias;

	while (h < len)
	{
		punycode_uint m = maxint;

		for (const wchar_t *ptr = str; ptr != end; ptr++)
			if (*ptr >= n && *ptr < m)
				m = *ptr;

		//if (m - n > (maxint - delta) / (h + 1))
		//	return punycode_overflow;

		delta += (m - n) * (h + 1);
		n = m;

		for (const wchar_t *ptr = str; ptr != end; ptr++)
		{
			if (*ptr < n)
				delta++;
				//if (delta == 0)
				//	return punycode_overflow;

			if (*ptr == n)
			{
				punycode_uint q = delta;

				for (punycode_uint k = base; ; k += base)
				{
					punycode_uint t = (k <= bias ? tmin
						: k >= bias + tmax ? tmax
						: k - bias);

					if (q < t)
						break;

					out.push_back( encode_digit(t + (q - t) % (base - t)) );
					q = (q - t) / (base - t);
				}

				out.push_back( encode_digit(q) );
				bias = adapt(delta, h + 1, h == b);
				delta = 0;
				h++;
			}
		}

		delta++;
		n++;
	}

	return out;
}

wstring punycode_decode(const char *str, size_t len)
{
	wstring out;
	const char *_str = str; /* Сохраняем str для вывода в exception */
	const char *end = str + len; /* Конец строки */
	const char *ascii_end; /* Конец ascii */
	const char *begin; /* Начало punycode */

    /* Обычное (не punycode) значение */

	if (len <= 4 || memcmp(str, "xn--", 4) != 0)
		ascii_end = begin = end;

    /* Punycode */
	else
	{
		str += 4; /* Пропускаем "xn--" */
		ascii_end = begin = str;
        /* Ищем - откуда начинается собственно punycode */
		for (const char *ptr = str; ptr != end; ptr++)
			if (*ptr == '-')
			{
				ascii_end = ptr;
				begin = ptr + 1;
			}
	}

	for (const char *ptr = str; ptr < ascii_end; ptr++)
	{
		if ( *ptr < 0 || *ptr > 0x7F)
			throw my::exception(L"Не удаётся декодировать punycode")
				<< my::param(L"value", _str);

		out.push_back((wchar_t)*ptr);
    }

    size_t out_sz = out.size();
	punycode_uint n = initial_n;
	punycode_uint i = 0;
	punycode_uint bias = initial_bias;

	for (const char *ptr = begin; ptr < end; out_sz++)
    {
		punycode_uint oldi = i;

		for (punycode_uint w = 1, k = base; ; k += base)
		{
			if (ptr == end)
				throw my::exception(L"Не удаётся декодировать punycode")
					<< my::param(L"value", _str);
			
			punycode_uint digit = decode_digit(*ptr++);

			if (digit >= base)
				throw my::exception(L"Не удаётся декодировать punycode")
					<< my::param(L"value", _str);
		
			if (digit > (maxint - i) / w)
				throw my::exception(L"Не удаётся декодировать punycode (overflow)")
					<< my::param(L"value", _str);
			
			i += digit * w;

			punycode_uint t = (k <= bias ? tmin
				: k >= bias + tmax ? tmax
				: k - bias);

			if (digit < t)
				break;

			if (w > maxint / (base - t))
				throw my::exception(L"Не удаётся декодировать punycode (overflow)")
					<< my::param(L"value", _str);
			
			w *= (base - t);
		}

		bias = adapt(i - oldi, out_sz + 1, oldi == 0);

		if (i / (out_sz + 1) > maxint - n)
			throw my::exception(L"Не удаётся декодировать punycode (overflow)")
				<< my::param(L"value", _str);

		n += i / (out_sz + 1);
		i %= (out_sz + 1);

		out.insert(i++, 1, (wchar_t)n);
	}

	return out;
}

}
