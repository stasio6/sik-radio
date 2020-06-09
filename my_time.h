#ifndef DUZE_MY_TIME_H
#define DUZE_MY_TIME_H

#include <sys/time.h>

timeval time_now();

// Converts given time to microseconds.
long long to_usec(timeval t);

// Calculates number of microseconds that passes from time t.
long long microseconds_passed_from(timeval t);

// Calculates time left to a given timeout, if last action was taken
// in time moment @from.
timeval time_left(timeval from, int timeout);

#endif //DUZE_MY_TIME_H
