#ifndef MY_LOG_H
#define MY_LOG_H

#include <string>

#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/format/format_fwd.hpp>

namespace my
{

class log
{
public:
	typedef boost::function<void (const std::wstring &title,
		const std::wstring &text)> on_log_t;

private:
	on_log_t on_log_;

public:
	log(on_log_t on_log)
		: on_log_(on_log) {}

	log& operator ()(const std::wstring &title,
		const std::wstring &text = std::wstring())
	{
		if (on_log_)
			on_log_(title, text);
		return *this;
	}

	log& operator ()(const std::wstring &title,
		const boost::wformat fmt)
	{
		if (on_log_)
			on_log_(title, fmt.str());
		return *this;
	}
};

}

#endif
