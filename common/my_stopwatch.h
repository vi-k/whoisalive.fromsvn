#ifndef MY_STOPWATCH_H
#define MY_STOPWATCH_H

#include <ostream>

#include "my_time.h"

namespace my {

class stopwatch
{
public:
	enum {show_count=1, show_total=2, show_avg=4,
		show_min=8, show_max=16, show_all=31};

private:
	posix_time::ptime start_;
	int show_;

public:
	int count;
	posix_time::time_duration total;
	posix_time::time_duration min;
	posix_time::time_duration max;

	stopwatch(int show = show_all)
		: count(0)
		, show_(show) {}

	void reset()
	{
		*this = stopwatch();
	}

	inline void start()
	{
		start_ = posix_time::microsec_clock::local_time();
	}

	inline void finish()
	{
		posix_time::time_duration time
			= posix_time::microsec_clock::local_time() - start_;

		if (time < min || count == 0)
			min = time;
		if (time > max || count == 0)
			max = time;

		total += time;
		count++;
	}

	inline posix_time::time_duration avg() const
	{
		return count ? (total / count)
			: posix_time::time_duration();
	}

	template<class Char>
	friend std::basic_ostream<Char>& operator <<(std::basic_ostream<Char>& out,
		const stopwatch &sw)
	{
		if (sw.count == 0)
			out << "null";
		else
		{
			if (sw.show_ & show_count)
				out	<< "count=" << sw.count;
			if (sw.show_ & show_total)
				out << " total=" << sw.total;
			if (sw.show_ & show_avg)
				out << " avg=" << sw.avg();
			if (sw.show_ & show_min)
				out << " min=" << sw.min;
			if (sw.show_ & show_max)
				out << " max=" << sw.max;
		}

		return out;
	}	
};

}

#endif
