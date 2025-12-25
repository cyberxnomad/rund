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
 * @brief Lock pid file
 *
 * @param fd file descriptor of the PID file
 * @return int
 */
static int lock_pid_file(int fd)
{
    struct flock fl = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0,
    };

    return fcntl(fd, F_SETLK, &fl);
}

/**
 * @brief Check if another instance is already running
 *
 * @param pid_file Path to the PID file.
 * @return int
 * @retval `fd` file descriptor of the PID file on success
 * @retval `-1` failure (file error or already running)
 */
int test_running(const char *pid_file)
{
    int fd = open(pid_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
    {
        fprintf(stderr, "failed to open %s: %s\n", pid_file, strerror(errno));
        return -1;
    }

    // try to lock pid file
    if (lock_pid_file(fd) < 0)
    {
        fprintf(stderr, "failed to lock %s (already running?): %s\n", pid_file, strerror(errno));

        close(fd);
        return -1;
    }

    // write PID to pid file
    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);
    dprintf(fd, "%d\n", getpid());

    return fd;
}

/**
 * @brief Daemonize current process
 *
 * @param pid_file Path to the PID file. If not NULL, it is used to ensure single instance
 *                 execution and store the process PID.
 * @return int
 * @retval `0` success (if pid_file is NULL)
 * @retval `>0` success (returns the PID file descriptor)
 * @retval `-1` failed
 */
int daemonize(const char *pid_file)
{
    pid_t pid;
    int pipefd[2];
    int pid_fd = 0;

    if (pid_file)
    {
        pid_fd = test_running(pid_file);
        if (pid_fd < 0)
        {
            return -1;
        }

        if (pipe(pipefd) < 0)
        {
            fprintf(stderr, "failed to create pipe: %s\n", strerror(errno));
            return -1;
        }
    }

    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "failed to fork: %s\n", strerror(errno));

        if (pid_file)
        {
            close(pipefd[0]);
            close(pipefd[1]);
        }

        return -1;
    }
    else if (pid > 0)
    {
        // here is parent process

        if (pid_file)
        {
            char sync_w = 0;
            close(pipefd[0]);
            // closing pid file to release lock
            close(pid_fd);

            write(pipefd[1], &sync_w, 1);
            close(pipefd[1]);
        }

        exit(EXIT_SUCCESS);
    }

    // here is child process, pid is 0
    if (pid_file)
    {
        char sync_r;
        close(pipefd[1]);
        close(pid_fd);

        // waiting parent process to exit
        read(pipefd[0], &sync_r, 1);
        close(pipefd[0]);

        // lock again
        pid_fd = test_running(pid_file);
        if (pid_fd < 0)
        {
            return -1;
        }
    }

    // detach previously process session
    setsid();
    umask(0);
    chdir("/");

    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd < 0)
    {
        syslog(LOG_ERR, "failed to open /dev/null: %s", strerror(errno));

        if (pid_file)
        {
            close(pid_fd);
            unlink(pid_file);
        }

        return -1;
    }

    // redirect stdin, stdout and stderr to /dev/null
    dup2(null_fd, STDIN_FILENO);
    dup2(null_fd, STDOUT_FILENO);
    dup2(null_fd, STDERR_FILENO);

    return pid_fd;
}
