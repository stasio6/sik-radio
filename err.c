#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "err.h"

void syserr(const char *fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");
    va_start(fmt_args, fmt);
    fprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    exit(EXIT_FAILURE);
}

void fatal(const char *fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");
    va_start(fmt_args, fmt);
    fprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    exit(EXIT_FAILURE);
}