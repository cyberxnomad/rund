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
 *
 */

#ifndef _INTERNAL_H_
#define _INTERNAL_H_

#include <stdbool.h>
#include <stdlib.h>

typedef struct
{
    char *stdout_file;
    char *stderr_file;
    char *working_dir;
    char **environments;
    size_t environment_cnt;
    bool respawn;
    int *respawn_codes;
    size_t respawn_code_cnt;
    int respawn_delay;
    int max_respawn_cnt;

    char *target;
    int target_argc;
    char **target_argv;
} option_t;

#define OPTION_INITIALIZER {NULL, NULL, NULL, \
                            NULL, 0,          \
                            false, NULL, 0,   \
                            3, 0,             \
                            NULL, 0, NULL}

void free_option(option_t *opt);
int parse_option(int argc, char **argv, option_t *opt);

typedef struct
{
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
} stdfds_t;

#define STDFDS_INITIALIZER {-1, -1, -1}

int daemonize(void);

#endif // _INTERNAL_H_
