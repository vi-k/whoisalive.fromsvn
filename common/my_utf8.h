#ifndef MY_UTF8_H
#define MY_UTF8_H

#include <wchar.h>
#include <string>
#include <boost/archive/detail/utf8_codecvt_facet.hpp>

namespace my {

class utf8 : public boost::archive::detail::utf8_codecvt_facet
{
	public:
		static std::wstring decode(const char *buf, size_t len)
		{
			static const utf8 utf8_decoder;

			std::wstring res(len, L' ');
			const char *from_next;
			wchar_t *to = (wchar_t*)res.c_str();
			wchar_t *to_end = to + res.size();			
			wchar_t *to_next;
			std::mbstate_t st;

			utf8_decoder.do_in(st, buf, buf + len, from_next, to, to_end, to_next);
			res.resize(to_next - to);

			return res;
		}

		static std::wstring decode(const std::string &str)
		{
			return decode(str.c_str(), str.size());
		}

		static std::string encode(const wchar_t *buf, size_t len)
		{
			static const utf8 utf8_decoder;

			std::string res(len * sizeof(wchar_t), ' ');
			const wchar_t *from_next;
			char *to = (char*)res.c_str();
			char *to_end = to + res.size();			
			char *to_next;
			std::mbstate_t st;

			utf8_decoder.do_out(st, buf, buf + len, from_next, to, to_end, to_next);
			res.resize(to_next - to);

			return res;
		}

		static std::string encode(const std::wstring &str)
		{
			return encode(str.c_str(), str.size());
		}
};

}

#endif
