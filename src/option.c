// SPDX-License-Identifier: GPL-3.0
/**
 * @file option.c
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
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "internal.h"
#include "version.h"

enum
{
    OPT_STDOUT = 'o',
    OPT_STDERR = 'e',
    OPT_CHDIR = 'c',
    OPT_ENV = 'E',
    OPT_PIDFILE = 'p',
    OPT_RESPAWN = 'r',
    OPT_HELP = 'h',
    OPT_VERSION = 'V',
    OPT_RESPAWN_CODE = 256,
    OPT_RESPAWN_DELAY,
    OPT_MAX_RESPAWNS,
};

// short options
// use the "+" prefix to prevent getopt_long from rearranging the order of argv.
static const char *short_opts = "+o:e:c:E:p:rhV";

// long options
static const struct option long_opts[] = {
    {"stdout", required_argument, NULL, OPT_STDOUT},
    {"stderr", required_argument, NULL, OPT_STDERR},
    {"chdir", required_argument, NULL, OPT_CHDIR},
    {"env", required_argument, NULL, OPT_ENV},
    {"pidfile", required_argument, NULL, OPT_PIDFILE},
    {"respawn", no_argument, NULL, OPT_RESPAWN},
    {"respawn-code", required_argument, NULL, OPT_RESPAWN_CODE},
    {"respawn-delay", required_argument, NULL, OPT_RESPAWN_DELAY},
    {"max-respawns", required_argument, NULL, OPT_MAX_RESPAWNS},
    {"help", no_argument, NULL, OPT_HELP},
    {"version", no_argument, NULL, OPT_VERSION},
    {0, 0, 0, 0},
};

static const char usage_text[] = {
    "usage: %s [options...] <target> [target_args...]\n"
    "\n"
    "A lightweight daemonizer and process supervisor.\n"
    "\n"
    "Options:\n"
    " -o, --stdout=FILE          Redirect stdout to FILE (default: /dev/null)\n"
    " -e, --stderr=FILE          Redirect stderr to FILE (default: /dev/null)\n"
    " -c, --chdir=DIR            Change working directory to DIR\n"
    " -E, --env=NAME=VALUE       Set environment variable\n"
    "                              Can be used multiple times\n"
    " -p, --pidfile=FILE         Write PID to FILE\n"
    " -r, --respawn              Enable auto-respawn on exit\n"
    "     --respawn-code=CODE    Respawn only if exit code equals CODE\n"
    "                              Can be used multiple times\n"
    "                              Use -1 for any codes\n"
    "                              Default: any non-zero codes (if -r is set)\n"
    "     --respawn-delay=N      Wait N seconds before respawning (default: 3)\n"
    "     --max-respawns=N       Maximum respawn attempts (default: 0 = unlimited)\n"
    " -h, --help                 Display this help message and exit\n"
    " -V, --version              Show version information and exit\n",
};

/**
 * @brief Show usages
 *
 * @param prog_name program name
 */
static void show_usage(const char *prog_name)
{
    fprintf(stderr, usage_text, prog_name);
}

/**
 * @brief Append environment to option
 *
 * @param opt option
 * @param env environment
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int append_env(option_t *opt, const char *env)
{
    if (!env)
    {
        return 0;
    }

    char **temp = (char **)realloc(opt->environments, (opt->environment_cnt + 1) * sizeof(char **));
    if (!temp)
    {
        fprintf(stderr, "%s", strerror(errno));
        return -1;
    }

    opt->environments = temp;
    opt->environments[opt->environment_cnt] = strdup(env);

    opt->environment_cnt++;

    return 0;
}

/**
 * @brief Parse respawn code
 *
 * @param opt option
 * @param code_str code string
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int parse_respawn_code(option_t *opt, const char *code_str)
{
    if (!code_str)
    {
        return 0;
    }

    char *endptr = NULL;
    long code = strtol(code_str, &endptr, 10);
    if (errno == ERANGE || code > 127 || code < -1)
    {
        fprintf(stderr, "failed to parse respawn code '%s': out of range [-1, 127]\n", code_str);
        return -1;
    }
    else if (code_str == endptr || *endptr != '\0')
    {
        fprintf(stderr, "failed to parse respawn code '%s': not a number\n", code_str);
        return -1;
    }

    if (code == -1)
    {
        // -1 means any status code
        for (int i = 0; i < RESPAWN_CODE_BITS_ARRAY_SIZE; i++)
        {
            opt->respawn_code_bits[i] = -1U;
        }
    }
    else
    {
        for (int i = 0; i < RESPAWN_CODE_BITS_ARRAY_SIZE; i++)
        {
            if (code < 32)
            {
                opt->respawn_code_bits[i] |= 1 << code;
                break;
            }

            code -= 32;
        }
    }

    return 0;
}

/**
 * @brief Parse respawn delay
 *
 * @param opt option
 * @param delay_str delay string
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int parse_respawn_delay(option_t *opt, const char *delay_str)
{
    if (!delay_str)
    {
        return 0;
    }

    char *endptr = NULL;
    long delay = strtol(delay_str, &endptr, 10);
    if (errno == ERANGE || delay < 0)
    {
        fprintf(stderr, "failed to parse respawn delay '%s': out of range\n", delay_str);
        return -1;
    }
    else if (delay_str == endptr || *endptr != '\0')
    {
        fprintf(stderr, "failed to parse respawn delay '%s': not a number\n", delay_str);
        return -1;
    }

    opt->respawn_delay = delay;

    return 0;
}

/**
 * @brief Parse max respawns
 *
 * @param opt option
 * @param count_str count string
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int parse_max_respawn_count(option_t *opt, const char *count_str)
{
    if (!count_str)
    {
        return 0;
    }

    char *endptr = NULL;
    long count = strtol(count_str, &endptr, 10);
    if (errno == ERANGE || count < 0)
    {
        fprintf(stderr, "failed to parse max respawns '%s': out of range\n", count_str);
        return -1;
    }
    else if (count_str == endptr || *endptr != '\0')
    {
        fprintf(stderr, "failed to parse max respawns '%s': not a number\n", count_str);
        return -1;
    }

    opt->max_respawn_cnt = count;

    return 0;
}

/**
 * @brief Parse file path
 *
 * @param dst pointer to store allocated path string
 * @param file file path
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int general_parse_file(char **dst, const char *file)
{
    if (!file)
    {
        return -1;
    }

    int rc;
    char buf1[PATH_MAX + 1];
    char buf2[PATH_MAX + 1];
    char buf3[PATH_MAX + 1];
    char *dir;
    char *base_name;
    struct stat st;

    strncpy(buf1, file, PATH_MAX);
    strncpy(buf2, file, PATH_MAX);

    base_name = basename(buf1);
    dir = dirname(buf2);

    dir = realpath(dir, buf3);
    if (!dir)
    {
        fprintf(stderr, "%s: %s\n", buf2, strerror(errno));
        return -1;
    }

    rc = access(dir, F_OK | X_OK);
    if (rc != 0)
    {
        fprintf(stderr, "%s: %s\n", dir, strerror(errno));
        return -1;
    }

    rc = stat(dir, &st);
    if (rc != 0)
    {
        fprintf(stderr, "%s: %s\n", dir, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "%s: not a directory\n", dir);
        return -1;
    }

    snprintf(buf2, sizeof(buf2), "%s/%s", dir, base_name);

    if (*dst)
    {
        free(*dst);
    }

    *dst = strdup(buf2);

    return 0;
}

/**
 * @brief Parse stderr file path
 *
 * @param opt option
 * @param file file path
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int parse_stdout_file(option_t *opt, const char *file)
{
    return general_parse_file(&opt->stdout_file, file);
}

/**
 * @brief Parse stderr file path
 *
 * @param opt option
 * @param file file path
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int parse_stderr_file(option_t *opt, const char *file)
{
    return general_parse_file(&opt->stderr_file, file);
}

/**
 * @brief Parse working directory
 *
 * @param opt option
 * @param dir working directory
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int parse_working_dir(option_t *opt, const char *dir)
{
    if (!dir)
    {
        return -1;
    }

    int rc;
    char abs_path_buf[PATH_MAX];
    char *abs_path;
    struct stat st;

    abs_path = realpath(dir, abs_path_buf);
    if (!abs_path)
    {
        fprintf(stderr, "%s: %s\n", dir, strerror(errno));
        return -1;
    }

    rc = access(abs_path, F_OK | X_OK);
    if (rc != 0)
    {
        fprintf(stderr, "%s: %s\n", abs_path, strerror(errno));
        return -1;
    }

    rc = stat(abs_path, &st);
    if (rc != 0)
    {
        fprintf(stderr, "%s: %s\n", abs_path, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "%s: not a directory\n", abs_path);
        return -1;
    }

    if (opt->working_dir)
    {
        free(opt->working_dir);
    }

    opt->working_dir = strdup(abs_path);

    return 0;
}

/**
 * @brief Parse pid file path
 *
 * @param opt option
 * @param file file path
 * @return int
 * @retval `0` ok
 * @retval `-1` failed
 */
static int parse_pid_file(option_t *opt, const char *file)
{
    return general_parse_file(&opt->pid_file, file);
}

/**
 * @brief Check whether the target program is valid
 *
 * @param target target program path
 * @return int
 * @retval `0` valid
 * @retval `-1` invalid
 */
static int check_target(const char *target)
{
    if (!target)
    {
        return -1;
    }

    int rc;
    struct stat st;

    if (target[0] != '/')
    {
        fprintf(stderr, "target must be an absolute path.\n");
        return -1;
    }

    rc = access(target, F_OK | X_OK);
    if (rc != 0)
    {
        fprintf(stderr, "%s: %s\n", target, strerror(errno));
        return -1;
    }

    rc = stat(target, &st);
    if (rc != 0)
    {
        fprintf(stderr, "%s: %s\n", target, strerror(errno));
        return -1;
    }

    if (!S_ISREG(st.st_mode))
    {
        fprintf(stderr, "%s: not a file\n", target);
        return -1;
    }

    return 0;
}

/**
 * @brief Free option
 *
 * @param opt option
 */
void free_option(option_t *opt)
{
    if (opt->stdout_file)
    {
        free(opt->stdout_file);
        opt->stdout_file = NULL;
    }

    if (opt->stderr_file)
    {
        free(opt->stderr_file);
        opt->stderr_file = NULL;
    }

    if (opt->working_dir)
    {
        free(opt->working_dir);
        opt->working_dir = NULL;
    }

    if (opt->environments)
    {
        for (int i = 0; i < opt->environment_cnt; i++)
        {
            free(opt->environments[i]);
        }

        free(opt->environments);
        opt->environments = NULL;
        opt->environment_cnt = 0;
    }

    if (opt->pid_file)
    {
        free(opt->pid_file);
        opt->pid_file = NULL;
    }

    memset(opt->respawn_code_bits, 0, sizeof(opt->respawn_code_bits));

    opt->respawn = false;
    opt->target = NULL;
    opt->target_argc = 0;
    opt->target_argv = NULL;
}

/**
 * @brief Parse option
 *
 * @param argc arg count
 * @param argv arg values
 * @param opt option buffer
 * @return int
 * @retval `1` ok and able to continue running
 * @retval `0` ok but should exit normally
 * @retval `-1` failed
 */
int parse_option(int argc, char **argv, option_t *opt)
{
    int rc;
    int cur;
    char *prog_name = basename(argv[0]);
    bool has_respawn_code = false;

    while ((cur = getopt_long(argc, argv, short_opts, long_opts, NULL)) != EOF)
    {
        rc = 0;

        switch (cur)
        {
        case OPT_STDOUT:
            rc = parse_stdout_file(opt, optarg);
            break;

        case OPT_STDERR:
            rc = parse_stderr_file(opt, optarg);
            break;

        case OPT_CHDIR:
            rc = parse_working_dir(opt, optarg);
            break;

        case OPT_ENV:
            rc = append_env(opt, optarg);
            break;

        case OPT_PIDFILE:
            rc = parse_pid_file(opt, optarg);
            break;

        case OPT_RESPAWN:
            opt->respawn = true;
            break;

        case OPT_RESPAWN_CODE:
            if (!has_respawn_code)
            {
                // clear default values
                memset(opt->respawn_code_bits, 0, sizeof(opt->respawn_code_bits));
                has_respawn_code = true;
            }
            rc = parse_respawn_code(opt, optarg);
            break;

        case OPT_RESPAWN_DELAY:
            rc = parse_respawn_delay(opt, optarg);
            break;

        case OPT_MAX_RESPAWNS:
            rc = parse_max_respawn_count(opt, optarg);
            break;

        case OPT_VERSION:
            printf("%s\n", VERSION_NAME);
            return 0;
            break;

        case OPT_HELP:
            show_usage(prog_name);
            return 0;
            break;

        default:
            show_usage(prog_name);
            rc = -1;
            break;
        }

        if (rc < 0)
        {
            return rc;
        }
    }

    if (optind >= argc)
    {
        fprintf(stderr, "error: missing target program\n");
        show_usage(prog_name);
        return -1;
    }

    rc = check_target(argv[optind]);
    if (rc < 0)
    {
        return rc;
    }

    opt->target = argv[optind];
    opt->target_argc = argc - optind;
    opt->target_argv = &argv[optind];

    return 1;
}
