// SPDX-License-Identifier: GPL-3.0
/**
 * @file main.c
 * @brief brief
 * @details details
 * @author Frank <uuidxx@163.com>
 *
 * @section Changelog
 * Date         Author                          Notes
 * 2025-12-19   Frank <uuidxx@163.com>          the first version
 * 2025-12-25   Frank <uuidxx@163.com>          add support for running user
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include "internal.h"

// Exit code used by the child process when execv() fails.
// This is a reserved internal status code (254) to distinguish between
// a failure in the supervisor's setup and the target program's own exit status.
// Value 254 is used as it is rarely used by standard applications.
#define CHILD_EXEC_ERR_CODE 254

static option_t option = OPTION_INITIALIZER;
static runtimefds_t runtimefds = RUNTIMEFDS_INITIALIZER;

static volatile sig_atomic_t shutdown_requested = 0;

/**
 * @brief Check if the target process should be respawned based on exit code
 *
 * @param opt option
 * @param code exit code of the terminated target process
 * @return bool
 * @retval `true` target should be respawned
 * @retval `false` target should not be respawned
 */
static bool check_respawn_required(const option_t *opt, int code)
{
    if (!opt->respawn)
    {
        return false;
    }

    for (int i = 0; i < RESPAWN_CODE_BITS_ARRAY_SIZE; i++)
    {
        if (code < 32)
        {
            return opt->respawn_code_bits[i] & (1 << code);
        }

        code -= 32;
    }

    return false;
}

/**
 * @brief Clean up resources and terminate the process
 *
 * @param code exit status code
 */
static void cleanup_and_exit(int code)
{
    if (runtimefds.stdout_fd >= 0)
    {
        close(runtimefds.stdout_fd);
        runtimefds.stdout_fd = -1;
    }

    if (runtimefds.stderr_fd >= 0)
    {
        close(runtimefds.stderr_fd);
        runtimefds.stderr_fd = -1;
    }

    if (runtimefds.pid_fd >= 0)
    {
        close(runtimefds.pid_fd);
        runtimefds.pid_fd = -1;

        unlink(option.pid_file);
    }

    free_option(&option);

    exit(code);
}

/**
 * @brief Redirect standard file descriptors (stdin, stdout, stderr)
 *
 * @param opt option
 * @param fds standard file descriptor
 */
static void redirect_std_fds(const option_t *opt, runtimefds_t *fds)
{
    if (option.stdout_file)
    {
        fds->stdout_fd = open(option.stdout_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fds->stdout_fd >= 0)
        {
            dup2(fds->stdout_fd, STDOUT_FILENO);
        }
        else
        {
            syslog(LOG_ERR, "failed to open %s: %s", option.stdout_file, strerror(errno));
        }
    }

    if (option.stderr_file)
    {
        fds->stderr_fd = open(option.stderr_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fds->stderr_fd >= 0)
        {
            dup2(fds->stderr_fd, STDERR_FILENO);
        }
        else
        {
            syslog(LOG_ERR, "failed to open %s: %s", option.stderr_file, strerror(errno));
        }
    }
}

/**
 * @brief Set user and group
 *
 * @param opt option
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int set_user_and_group(const option_t *opt)
{
    if (!opt->user)
    {
        return 0;
    }

    int rc;

    rc = initgroups(opt->user, opt->gid);
    if (rc < 0)
    {
        syslog(LOG_ERR, "failed to init groups: %s", strerror(errno));
        return -1;
    }

    rc = setgid(opt->gid);
    if (rc < 0)
    {
        syslog(LOG_ERR, "failed to set group: %s", strerror(errno));
        return -1;
    }

    rc = setuid(opt->gid);
    if (rc < 0)
    {
        syslog(LOG_ERR, "failed to set user: %s", strerror(errno));
        return -1;
    }

    setenv("USER", opt->user, 1);
    setenv("LOGNAME", opt->user, 1);
    setenv("HOME", opt->home_dir, 1);

    return 0;
}

/**
 * @brief Set the environments
 *
 * @param opt option
 */
static void set_environments(const option_t *opt)
{
    for (int i = 0; i < opt->environment_cnt; i++)
    {
        putenv(opt->environments[i]);
    }
}

/**
 * @brief Gracefully shut down the target process
 *
 * Attempts to terminate the target process gracefully by sending signals.
 * First sends SIGTERM, and if the process doesn't exit within the grace period,
 * sends SIGKILL as a last resort.
 *
 * @param pid process ID of the target to shut down
 * @param opt option
 */
static void graceful_shutdown(pid_t pid, const option_t *opt)
{
    if (pid <= 0)
    {
        return;
    }

    int rc;
    // 10 seconds
    // *improve: make it configurable
    int timeout_cnt = 5 * 10;

    // send SIGTERM first
    kill(pid, SIGTERM);

    while (timeout_cnt > 0)
    {
        usleep(200 * 1000);
        rc = waitpid(pid, NULL, WNOHANG);
        if (rc == pid)
        {
            return;
        }

        timeout_cnt--;
    }

    // timed out and force to kill
    syslog(LOG_WARNING, "waiting for %s to exit timed out; force terminating it", opt->target);
    kill(pid, SIGKILL);

    waitpid(pid, NULL, 0);
}

/**
 * @brief Signal handler for process management
 *
 * @param sig_no signal number received
 */
static void sigaction_handler(int sig_no)
{
    syslog(LOG_WARNING, "exit signal received: %s (%d)", strsignal(sig_no), sig_no);

    shutdown_requested = 1;
}

/**
 * @brief Initialize signal handlers
 *
 */
static void sigaction_init()
{
    struct sigaction sa;

    sa.sa_handler = sigaction_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_flags |= SA_RESTART;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int main(int argc, char **argv)
{
    int rc;
    char *prog_name = basename(argv[0]);

    rc = parse_option(argc, argv, &option);
    switch (rc)
    {
    case -1:
        cleanup_and_exit(EXIT_FAILURE);
        break;

    case 0:
        cleanup_and_exit(EXIT_SUCCESS);
        break;

    default:
        break;
    }

    openlog(prog_name, LOG_PID, LOG_DAEMON);

    rc = daemonize(option.pid_file);
    if (rc < 0)
    {
        cleanup_and_exit(EXIT_FAILURE);
    }
    if (rc > 0)
    {
        runtimefds.pid_fd = rc;
    }

    pid_t pid;
    int status;
    bool respawn_required;
    unsigned int respawn_cnt = 0;

    while (1)
    {
        pid = fork();
        if (pid < 0)
        {
            syslog(LOG_ERR, "failed to fork: %s", strerror(errno));

            syslog(LOG_ERR, "%s exited", prog_name);
            cleanup_and_exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // here is child process

            setsid();
            umask(0);

            if (option.working_dir)
            {
                chdir(option.working_dir);
            }

            set_environments(&option);

            redirect_std_fds(&option, &runtimefds);

            // switch user
            rc = set_user_and_group(&option);
            if (rc < 0)
            {
                exit(CHILD_EXEC_ERR_CODE);
            }

            syslog(LOG_INFO, "start to execute %s", option.target);

            execv(option.target, option.target_argv);

            exit(CHILD_EXEC_ERR_CODE);
        }

        // here is parent process

        sigaction_init();

        while (1)
        {
            if (shutdown_requested)
            {
                syslog(LOG_INFO, "graceful shutdown %s", option.target);

                graceful_shutdown(pid, &option);

                syslog(LOG_INFO, "%s exited", prog_name);
                cleanup_and_exit(EXIT_SUCCESS);
            }

            // wait for child process to exit
            rc = waitpid(pid, &status, WNOHANG);

            if (rc == pid)
            {
                respawn_required = option.respawn;

                // check the exit status of the child process
                if (WIFEXITED(status))
                {
                    // check if execution failed
                    if (WEXITSTATUS(status) == CHILD_EXEC_ERR_CODE)
                    {
                        syslog(LOG_ERR, "failed to execute %s", option.target);
                        syslog(LOG_ERR, "%s exited", prog_name);
                        cleanup_and_exit(EXIT_FAILURE);
                    }

                    // child exited normally
                    // check if respawn is needed
                    syslog(LOG_WARNING, "%s exited, status: %d", option.target, WEXITSTATUS(status));
                    respawn_required = check_respawn_required(&option, WEXITSTATUS(status));
                }
                else if (WIFSIGNALED(status))
                {
                    // child terminated by a signal
                    syslog(LOG_WARNING, "%s exited, signal: %s (%d)", option.target, strsignal(WTERMSIG(status)), WTERMSIG(status));
                }
                else
                {
                    // child exited abnormally without a clear signal or exit status
                    syslog(LOG_WARNING, "%s exited abnormal", option.target);
                }

                // increment respawn counter and check against the configured maximum
                respawn_cnt++;
                if (option.max_respawn_cnt && respawn_cnt > option.max_respawn_cnt)
                {
                    syslog(LOG_INFO, "maximum respawn attempts reached for %s", option.target);
                    syslog(LOG_INFO, "%s exited", prog_name);
                    cleanup_and_exit(EXIT_SUCCESS);
                }

                // exit if respawning is not required
                if (!respawn_required)
                {
                    syslog(LOG_INFO, "%s exited", prog_name);
                    cleanup_and_exit(EXIT_SUCCESS);
                }

                if (option.respawn_delay > 0)
                {
                    syslog(LOG_INFO, "%s respawning in %d seconds", option.target, option.respawn_delay);
                    sleep(option.respawn_delay);
                }
                else
                {
                    syslog(LOG_INFO, "%s respawning immediately", option.target);
                }

                // exit the loop to respawn the child process
                break;
            }

            usleep(200 * 1000);
        }
    }

    syslog(LOG_INFO, "%s exited", prog_name);
    cleanup_and_exit(EXIT_SUCCESS);
}
