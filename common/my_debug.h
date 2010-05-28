#ifndef MY_DEBUG_H
#define MY_DEBUG_H

#include "my_stopwatch.h"

namespace my {

#ifndef MY_STOPWATCH_DEBUG

#define MY_STOPWATCH(t)
#define MY_STOPWATCH_START(t)
#define MY_STOPWATCH_FINISH(t)
#define MY_STOPWATCH_OUT(out,t)
#define IF_MY_STOPWATCH(c)

#else

#define MY_STOPWATCH(t) my::stopwatch t;
#define MY_STOPWATCH_START(t) (t).start();
#define MY_STOPWATCH_FINISH(t) (t).finish();
#define MY_STOPWATCH_OUT(out,t) out << t;
#define IF_MY_STOPWATCH(c) c

#endif

}

#endif
