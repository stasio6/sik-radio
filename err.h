#ifndef DUZE_ERR_H
#define DUZE_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

// Throws a system error.
void syserr(const char *fmt, ...);

// Throws a fatal error.
void fatal(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif //DUZE_ERR_H
