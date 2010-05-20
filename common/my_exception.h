#ifndef MY_EXCEPTION_H
#define MY_EXCEPTION_H

#include "my_str.h"

#include <exception>
#include <string>
#include <sstream>
#include <ostream>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/format/format_fwd.hpp>

namespace my
{

struct param
{
	std::wstring key;
	std::wstring value;

	template<class T>
	param(const std::wstring &_key, const T& x, bool escape = true)
		: key(_key)
	{
		std::wstringstream out;
		out << x;
		value = out.str();
		if (escape)
			value = my::str::escape(value, my::str::escape_cntrl_only);
	}
};

class exception : public std::exception
{
protected:
	typedef std::vector<param> params_array;

	boost::wformat fmt_;
	params_array params;
	std::wstring last_key;
	std::wstring parent_message;

	public:
		exception(const std::wstring &fmt) throw()
			: fmt_(fmt) {}

		exception(const std::exception &e) throw()
		{
			message( my::str::to_wstring(e.what()) );
			//*this << my::param(L"typeid", typeid(e).name());
		}

		std::wstring message() const throw()
		{
			std::wstringstream out;
			out << fmt_.str();

			BOOST_FOREACH(const param &p, params)
				out << std::endl << p.key << L": " << p.value;

			if (!parent_message.empty())
				out << parent_message;

			return out.str();
		}

		void message(const std::wstring &new_message) throw()
		{
			fmt_.parse(new_message);
		}

		virtual const char *what() const throw()
		{
			static std::string out( my::str::to_string(message()) );
			return out.c_str();
		}

		exception& operator <<(const exception &e)
		{
			parent_message += L"\n-- my::exception --\n"
				+ e.message();
			return *this;
		}

		exception& operator <<(const std::exception &e)
		{
			parent_message += L"\n-- std::exception --\n"
				+ my::str::to_wstring(e.what());
				//+ L"\ntypeid: " + my::str::to_wstring(typeid(e).name());
			return *this;
		}

		exception& operator <<(const param &p)
		{
			params.push_back(p);
			return *this;
		}

		template<class T>
		exception& operator%(const T& x)
		{
			fmt_ % x;
			return *this;
		}
};

}

#endif
