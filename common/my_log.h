#ifndef MY_LOG_H
#define MY_LOG_H

#include <string>
#include <sstream>

#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/format/format_fwd.hpp>

namespace my
{

class log
{
public:
	typedef boost::function<void (const std::wstring &text)> on_log_proc;

private:
	on_log_proc on_log_;
	std::wstringstream out_;

public:
	log(on_log_proc on_log)
		: on_log_(on_log) {}

	/*-
	void operator ()(const std::wstring &text)
	{
		flush();
		out_ << text;
		flush();
	}
    -*/

	void operator <<(const log& x)
	{
		flush();
	}

	template<class T>
	log& operator <<(const T& x)
	{
		out_ << x;
		return *this;
	}

	void flush()
	{
		std::wstring text = out_.str();
		if (on_log_ && !text.empty())
			on_log_(text);
		out_.str(L"");
	}
};

}

#endif
