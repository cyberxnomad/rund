// SPDX-License-Identifier: GPL-3.0
/**
 * @file internal.h
 * @brief brief
 * @details details
 * @author Frank <uuidxx@163.com>
 *
 * @section Changelog
 * Date         Author                          Notes
 * 2025-12-22   Frank <uuidxx@163.com>          the first version
 * 2025-12-25   Frank <uuidxx@163.com>          add support for running user
 *
 */

#ifndef _INTERNAL_H_
#define _INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#define RESPAWN_CODE_BITS_ARRAY_SIZE 4
#define RESPAWN_CODE_BITS_ELEM_WIDTH 32

typedef struct
{
    char *stdout_file;
    char *stderr_file;

    char *working_dir;

    char *user;
    char *home_dir;
    uid_t uid;
    gid_t gid;

    char **environments;
    size_t environment_cnt;

    char *pid_file;

    bool respawn;
    uint32_t respawn_code_bits[RESPAWN_CODE_BITS_ARRAY_SIZE];
    int respawn_delay;
    int max_respawn_cnt;

    char *target;
    int target_argc;
    char **target_argv;
} option_t;

#define OPTION_INITIALIZER                         \
    {NULL /* stdout_file */,                       \
     NULL /* stderr_file */,                       \
     NULL /* working_dir */,                       \
     NULL /* user */,                              \
     NULL /* home_dir */,                          \
     0 /* uid */,                                  \
     0 /* gid */,                                  \
     NULL /* environments */,                      \
     0 /* environment_cnt */,                      \
     NULL /* pid_file */,                          \
     false /* respawn */,                          \
     {-2U, -1U, -1U, -1U} /* respawn_code_bits */, \
     3 /* respawn_delay */,                        \
     0 /* max_respawn_cnt */,                      \
     NULL /* target */,                            \
     0 /* target_argc */,                          \
     NULL /* target_argv */}

void free_option(option_t *opt);
int parse_option(int argc, char **argv, option_t *opt);

typedef struct
{
    int stdout_fd;
    int stderr_fd;
    int pid_fd;
} runtimefds_t;

#define RUNTIMEFDS_INITIALIZER {-1, -1, -1}

int daemonize(const char *pid_file);

enum LOG_LEVEL
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
};

int log_init(const char *ident);
void log_enable_syslog(void);

void log_log(enum LOG_LEVEL level, const char *fmt, ...);

#define log_debug(...) log_log(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define log_info(...)  log_log(LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_LEVEL_WARN, __VA_ARGS__)
#define log_error(...) log_log(LOG_LEVEL_ERROR, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_LEVEL_FATAL, __VA_ARGS__)

#endif // _INTERNAL_H_
