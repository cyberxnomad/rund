// SPDX-License-Identifier: GPL-3.0
/**
 * @file log.c
 * @brief brief
 * @details details
 * @author Frank <uuidxx@163.com>
 *
 * @section Changelog
 * Date         Author                          Notes
 * 2025-12-25   Frank <uuidxx@163.com>          the first version
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include "internal.h"

static const int syslog_levels[] = {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERR,
    LOG_CRIT,
};

static volatile bool syslog_enabled = false;

int log_init(const char *ident)
{
    openlog(ident, LOG_PID, LOG_DAEMON);

    return 0;
}

void log_enable_syslog(void)
{
    syslog_enabled = true;
}

void log_log(enum LOG_LEVEL level, const char *fmt, ...)
{
    if (level > LOG_LEVEL_FATAL)
    {
        return;
    }

    va_list args;

    if (syslog_enabled)
    {
        va_start(args, fmt);
        vsyslog(syslog_levels[level], fmt, args);
        va_end(args);

        return;
    }

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");

    fflush(stderr);
}