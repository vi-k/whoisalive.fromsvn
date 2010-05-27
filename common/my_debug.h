#ifndef MY_DEBUG_H
#define MY_DEBUG_H

#include "my_time.h"

namespace my { namespace debug {

class timer
{
private:
	posix_time::ptime start_;

public:		
	int count;
	posix_time::time_duration total;
	posix_time::time_duration min;
	posix_time::time_duration max;

	timer() : count(0) {}

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

	inline posix_time::time_duration avg()
	{
		return total / count;
	}
};

} }

#endif
