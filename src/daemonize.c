// SPDX-License-Identifier: GPL-3.0
/**
 * @file daemonize.c
 * @brief brief
 * @details details
 * @author Frank <uuidxx@163.com>
 *
 * @section Changelog
 * Date         Author                          Notes
 * 2025-12-22   Frank <uuidxx@163.com>          the first version
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/**
 * @brief Daemonize current process
 * 
 * @return int 
 * @retval `0` ok
 * @retval `-1` failed
 */
int daemonize(void)
{
    pid_t pid;

    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "failed to fork: %s", strerror(errno));

        return -1;
    }
    else if (pid > 0)
    {
        // here is parent process

        exit(EXIT_SUCCESS);
    }

    // here is child process, pid is 0

    // detach previously process session
    setsid();
    umask(0);
    chdir("/");

    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd < 0)
    {
        syslog(LOG_ERR, "failed to open /dev/null: %s", strerror(errno));
        return -1;
    }

    // redirect stdin, stdout and stderr to /dev/null
    dup2(null_fd, STDIN_FILENO);
    dup2(null_fd, STDOUT_FILENO);
    dup2(null_fd, STDERR_FILENO);

    return 0;
}
