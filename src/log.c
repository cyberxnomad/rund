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

// Mapping of log levels to syslog priorities
static const int syslog_levels[] = {
    LOG_DEBUG,   // LOG_LEVEL_DEBUG
    LOG_INFO,    // LOG_LEVEL_INFO
    LOG_WARNING, // LOG_LEVEL_WARN
    LOG_ERR,     // LOG_LEVEL_ERROR
    LOG_CRIT,    // LOG_LEVEL_FATAL
};

// Indicating whether syslog output is enabled
static volatile bool syslog_enabled = false;

/**
 * @brief Initialize logger
 *
 * @param ident program identity
 * @return int
 * @retval `0` ok
 */
int log_init(const char *ident)
{
    openlog(ident, LOG_PID, LOG_DAEMON);

    return 0;
}

/**
 * @brief Enable syslog output for log messages
 *
 */
void log_enable_syslog(void)
{
    syslog_enabled = true;
}

/**
 * @brief Log a message with the specified level and format
 *
 * @param level the log level (LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, etc.)
 * @param fmt the format string for the log message
 * @param ... variable arguments for the format string
 *
 * @note Messages with level > LOG_LEVEL_FATAL are silently ignored
 */
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