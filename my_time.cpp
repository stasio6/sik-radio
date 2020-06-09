#include "my_time.h"

timeval time_now() {
    timeval current_time;
    gettimeofday(&current_time, nullptr);
    return current_time;
}

timeval make_duration(int sec, int usec) {
    return timeval{sec, usec};
}

timeval time_diff(timeval t1, timeval t2) {
    timeval result = {t1.tv_sec - t2.tv_sec, t1.tv_usec - t2.tv_usec};
    if (result.tv_usec < 0) {
        result.tv_usec += 1000000;
        result.tv_sec--;
    }
    if (result.tv_sec < 0)
        return make_duration(0, 0);
    return result;
}

long long to_usec(timeval t) {
    return t.tv_sec * 1000000ll + t.tv_usec;
}

long long microseconds_passed_from(timeval t) {
    return to_usec(time_diff(time_now(), t));
}

timeval time_left(timeval from, int timeout) {
    timeval time_taken = time_diff(time_now(), from);
    return time_diff(make_duration(timeout, 0), time_taken);
}