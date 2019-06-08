//
// Created by iwant on 19-5-23.
//

/* ====================================================
 * TShell 一些支持：
 *    1) 支持使用 '' (单引号) 限定括号内字符串不作为标识符
 *      注意: '' 能不支持含有空白字符;
 *    2) 支持使用 export 设置环境变量
 *    3) 支持通过设置 CMD_SYM 环境变量修改命令提示符
 *      注意: 命令提示符支持的最大长度为 64
 *    4) 支持使用 < << > 完成重定向
 *    5) 支持使用 作业控制
 *    6) 支持使用 exit or ctrl+c 组合键用该做退出
 *    7) 支持使用上下箭头等快捷键(ctrl + r)翻阅或查找历史命令
 *    8) 支持 Tab 补全 ★★★
 *      注: 7 & 8 基于 readline 库实现
 * ====================================================
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <setjmp.h>
#include <dirent.h>
#include <sys/stat.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "shell.h"

static void init(int *p_sems_id);

int get_cmd_input(char *);

void change_cmd_symbol();

int is_shell_cmd(char *[]);

static void sigchld_handler(int);

static void sigint_handler(int);

static void sigwinch_handler(int);

static int move_to_high_fd(int);

char **shell_completion(const char *, int, int);

char *command_generator(const char *, int);

extern char **environ;

int fd_redirect_in = STDIN_FILENO;
int fd_redirect_out = STDOUT_FILENO;
int fd_redirect_err = STDERR_FILENO;

// shell 的 pgid
pid_t shell_pgid;

// shell 对应控制终端的文件描述符
int shell_fd;

static int save_fd_in = STDIN_FILENO;
static int save_fd_out = STDOUT_FILENO;
static int save_fd_err = STDERR_FILENO;

static sigjmp_buf jmpbuf;

// 命令提示符
static char cmd_symbol[MAX_CMD_SYM_LEN] = {DEFAULT_CMD_SYM};

/*
 * 命令是否是后台执行
 * 0  : 非后台运行
 * 1  : 后台运行
 */
static int is_background = 0;

// 暂存前台进程运行的命令
char cmd[MAX_CMD_LEN];

/*
 * 前缀树,提供 Tab 补全的命令检索
 * 注: 启动 shell 时构建此树,但在 shell 运行中不会该表此树.
 *     换句话说,只有在下次启动 shell 时,命令的更新才会体现在前缀树中.
 */
static trie *p_trie;

// 标识窗口大小是否更改过
static int sigwinch_received = 0;

int main(void) {
    pid_t pid;
    char cmd_input[MAX_CMD_LEN];
    int sems_id = -1;
    int exit_status;

    // 用于异常退出
    if ((exit_status = setjmp(jmpbuf)) != 0)
        goto label;

    // 初始化
    init(&sems_id);

    for (;;) {
        // 读取环境变量 CMD_SYM , 更改默认命令行提示符
        change_cmd_symbol();

        if (get_cmd_input(cmd_input) == 0) {
            // 重置窗口大小(美化 Tab 补全输出)
            if (sigwinch_received) {
                rl_resize_terminal();
                sigwinch_received = 0;
            }

            // 计算字符串中 “词” 的个数
            int len = get_word_num(cmd_input);
            if (len > 0) {
                char *words[len + 1];
                words[len] = NULL;
                // 空白字符分割字符串
                split_str(cmd_input, words);
                // 判断是否时 shell 的自己命令
                if (!is_shell_cmd(words)) {
                    if (words[0] != NULL) {
                        // 保存命令
                        int index = 0;
                        for (int i = 0; words[i] != NULL; ++i) {
                            strcpy(cmd + index, words[i]);
                            index += strlen(words[i]);
                            cmd[index++] = ' ';
                        }
                        cmd[--index] = '\0';

                        if ((pid = fork()) < 0)
                            // 创建子进程失败
                            perror("create child process");
                        else if (pid == 0) {
                            // 子进程

                            // 设置进程组
                            if (setpgid(getpid(), getpid()) != 0)
                                perror("Set process group");
                            int c_sems_id = get_sem(PATH_NAME, PROJ_ID);
                            v_op(c_sems_id, 0);
                            if (!is_background)
                                p_op(c_sems_id, 1);

                            // 重置信号屏蔽
                            sigset_t set, oset;
                            sigfillset(&set);
                            sigemptyset(&oset);
                            if (sigprocmask(SIG_UNBLOCK, &set, &oset) < 0) {
                                perror("signal mask");
                                exit(125);
                            }

                            // 更改有效用户ID 组ID
                            seteuid(getuid());
                            setegid(getgid());

                            // exec
                            execvp(words[0], words);

                            // exec 出错时才会执行此
                            // 恢复标准 IN OUT
                            dup2(save_fd_in, STDIN_FILENO);
                            dup2(save_fd_out, STDOUT_FILENO);
                            dup2(save_fd_err, STDERR_FILENO);
                            fprintf(stderr, "Couldn't execute: %s\n", cmd_input);
                            exit(124);
                        }
                        // 父进程
                        // 等待子进程设置完进程组
                        p_op(sems_id, 0);
                        if (!is_background) {
                            // 非后台运行
                            if (tcsetpgrp(shell_fd, getpgid(pid)) != 0) {
                                perror("Set foreground process group");
                                longjmp(jmpbuf, 123);
                            }
                            v_op(sems_id, 1);
                            pause();
                        } else
                            // 后台运行
                            add_job(getpgid(pid), RUNNING, cmd);
                        // 无论 非后台 or 后台运行，都在最后扫描作业队列，见
                    }
                }
                // 清理重定向时打开的文件描述符
                if (fd_redirect_in != STDIN_FILENO) {
                    if (fd_redirect_in != STDOUT_FILENO && fd_redirect_in != STDERR_FILENO)
                        close(fd_redirect_in);
                    fd_redirect_in = STDIN_FILENO;
                    dup2(save_fd_in, STDIN_FILENO);
                }
                if (fd_redirect_out != STDOUT_FILENO) {
                    if (fd_redirect_out != STDIN_FILENO && fd_redirect_out != STDERR_FILENO)
                        close(fd_redirect_out);
                    fd_redirect_out = STDOUT_FILENO;
                    dup2(save_fd_out, STDOUT_FILENO);
                }
                if (fd_redirect_err != STDERR_FILENO) {
                    if (fd_redirect_err != STDIN_FILENO && fd_redirect_err != STDOUT_FILENO)
                        close(fd_redirect_err);
                    fd_redirect_err = STDERR_FILENO;
                    dup2(save_fd_err, STDERR_FILENO);
                }
                // 清除 is_background，以便下次执行命令默认为前台执行
                is_background = 0;
            }
        }
        // 扫描作业队列
        scan_jobs();
    }

    label:

    // 释放信号量
    if (sems_id >= 0)
        destroy_sem(sems_id, 0);

    // 释放前缀树
    if (p_trie != NULL)
        trie_free(p_trie);

    return exit_status;
}

/*
 * 一些初始化
 */
static void init(int *p_sems_id) {
    // 获取 shell 的 pgid
    shell_pgid = getpgid(0);
    if (shell_pgid == -1) {
        perror("Get shell pgid");
        longjmp(jmpbuf, 127);
    }

    // 将 shell 的控制终端 fd 移置于文件描述符列表的最高处
    // 以减少重定向等对它的影响
    shell_fd = move_to_high_fd(STDIN_FILENO);

    // 复制 3 个标准描述符以方便恢复
    save_fd_in = dup(STDIN_FILENO);
    save_fd_out = dup(STDOUT_FILENO);
    save_fd_err = dup(STDERR_FILENO);

    /*
     * 创建并初始化信号量
     * 用于控制 父、子 进程执行顺序
     */
    *p_sems_id = create_sem(2, PATH_NAME, PROJ_ID);
    if (*p_sems_id < 0) {
        perror("create sem");
        longjmp(jmpbuf, 126);
    }
    init_sem(*p_sems_id, 0, 0);
    init_sem(*p_sems_id, 1, 0);

    // 注册 信号处理程序，用于作业控制
    struct sigaction new_act, old_act;
    new_act.sa_handler = sigchld_handler;
    sigemptyset(&new_act.sa_mask);
    new_act.sa_flags = 0;
    sigaction(SIGCHLD, &new_act, &old_act);
    // 注册 信号处理程序，用于支持 ctrl + c 退出
    new_act.sa_handler = sigint_handler;
    sigemptyset(&new_act.sa_mask);
    new_act.sa_flags = 0;
    sigaction(SIGINT, &new_act, &old_act);
    // 注册 信号处理程序，用于监听窗口大小的改变
    new_act.sa_handler = sigwinch_handler;
    sigemptyset(&new_act.sa_mask);
    new_act.sa_flags = 0;
    sigaction(SIGWINCH, &new_act, &old_act);

    // 信号屏蔽
    sigset_t old_sigset;
    sigset_t new_sigset;
    sigfillset(&new_sigset);
    sigdelset(&new_sigset, SIGCHLD);
    sigdelset(&new_sigset, SIGINT);
    sigdelset(&new_sigset, SIGWINCH);
    sigemptyset(&old_sigset);
    if (sigprocmask(SIG_BLOCK, &new_sigset, &old_sigset) < 0) {
        perror("sigprocmask");
        longjmp(jmpbuf, 125);
    }

    // 前缀树初始化
    p_trie = trie_create();

    int i = 0, last = 0;
    struct stat buf;
    char *dirs = getenv("PATH");
    char path[256];

    // 将 PATH 路径下的所有可执行文件的名称,添加至前缀树中
    while (dirs[i] != '\0') {
        if (dirs[i] == ':' && last != i) {
            memcpy(path, dirs + last, i - last);
            path[i - last] = '/';

            DIR *dp = opendir(path);
            struct dirent *dirp;
            if (dp != NULL) {
                // 遍历该文件夹
                while ((dirp = readdir(dp)) != NULL) {
                    strcpy(path + i - last + 1, dirp->d_name);
                    stat(path, &buf);
                    // 判断是否时可执行文件
                    if (S_ISREG(buf.st_mode) && access(path, X_OK) >= 0)
                        trie_insert(p_trie, dirp->d_name);
                }
            }
            memset(path, 0, sizeof(path));
            last = i + 1;
        }
        ++i;
    }

    /*
     * readline (lib) 初始化
     * 作用:
     *      1) tab 补全支持
     *      2) 提供历史命令支持
     */
    rl_readline_name = "TShell";
    rl_attempted_completion_function = shell_completion;
}

/*
 * 获取输入的 “命令”
 *
 * @param cmd_input
 * @return 获取成功，返回 0。否则，获取失败返回 -1, 命令过长返回 1。
 */
int get_cmd_input(char *cmd_input) {
    char *line = readline(cmd_symbol);
    if (line != NULL && line[0] != '\0') {
        if (strlen(line) < MAX_CMD_LEN) {
            // 用于支持历史消息
            add_history(line);

            strcpy(cmd_input, line);
            free(line);
            return 0;
        } else {
            fprintf(stderr, "Command too length!");
            free(line);
            return 1;
        }
    }
    return -1;
}

/*
 * 更改命令提示符
 */
void change_cmd_symbol() {
    char *tp_cmd_symbol = getenv(CMD_SYM);
    if (tp_cmd_symbol != NULL && strlen(tp_cmd_symbol) < MAX_CMD_SYM_LEN - 1) {
        strcpy(cmd_symbol, tp_cmd_symbol);
        int len = strlen(cmd_symbol);
        cmd_symbol[len] = ' ';
        cmd_symbol[len + 1] = '\0';
    }
}

/*
 * 判断是否是 shell 内部命令
 */
int is_shell_cmd(char *words[]) {
    for (int i = 0; words[i] != NULL; ++i) {
        if (strcmp(words[i], REDIRECT_INPUT_SYM) == 0) {
            // 处理输入重定向
            int fd = process_input_redirect(words);
            if (fd < 0)
                return 1;
            else {
                fd_redirect_in = fd;

                // 设置标准输入重定向
                dup2(fd_redirect_in, STDIN_FILENO);
            }
            // 由于在处理重定向过程中会对 words 进行修改（去掉已处理的与重定向相关的）
            // 所以，每次处理完后进行回溯。这样可能造成一点点性能损耗。下同
            i = -1;
        } else if (strcmp(words[i], REDIRECT_OUTPUT_SYM) == 0) {
            // 处理输出重定向（截断）
            if (process_output_redirect(words) < 0)
                return 1;
            i = -1;
        } else if (strcmp(words[i], REDIRECT_OUTPUT_APPEND_SYM) == 0) {
            // 处理输出重定向（追加）
            if (process_output_append_redirect(words) < 0)
                return 1;
            i = -1;
        } else if (strcmp(words[i], BACKGROUND_SYM) == 0) {
            // 后台执行
            process_backstage_execution(words);
            is_background = 1;
        }
    }

    char *p_cmd = words[0];
    if (strcmp(p_cmd, "export") == 0) {
        exec_export(words);
        return 1;
    } else if (strcmp(p_cmd, "jobs") == 0) {
        exec_jobs(words);
        return 1;
    } else if (strcmp(p_cmd, "fg") == 0) {
        exec_fg(words);
        return 1;
    } else if (strcmp(p_cmd, "bg") == 0) {
        exec_bg(words);
        return 1;
    } else if (strcmp(p_cmd, "exit") == 0) {
        printf("bye bye~~\n");
        longjmp(jmpbuf, 1);
    }

    // 清理：去除参数中的 ''，如 '>>' 清理后变为 >>
    for (int i = 0; words[i] != NULL; ++i)
        clean_char_in_word('\'', words[i]);
    return 0;
}


/*
 * 处理 SIGCHLD 信号
 * 注：用于作业控制，将前台进程挂起
 */
static void sigchld_handler(int signo) {
    int pgid = tcgetpgrp(shell_fd);
    if (pgid == -1) {
        perror("get fg process group");
        return;
    }
    /* ★★★
     * 此处屏蔽了后台运行中的暂停、退出等 --> 交给 scan_jobs
     */
    if (pgid == shell_pgid)
        return;
    int status;
    int ret = waitpid(pgid, &status, WUNTRACED);
    if (ret != -1) {
        if (WIFSTOPPED(status)) {    /* 命令前台运行中停止 */
            // 给前台进程组发送 SIGSTOP 信号，使其暂停
            if (killpg(pgid, SIGSTOP) != 0) {
                perror("fail to send signal");
                return;
            }
            // 设置 shell 所在进程组为前台进程组
            if (give_terminal_to(shell_pgid) != 0) {
                fprintf(stderr, "Error: fail to set fg.\n");
                longjmp(jmpbuf, 122);
            }
            // 添加到作业队列
            add_job(pgid, STOP, cmd);
        } else if (WIFEXITED(status) || WIFSIGNALED(status))    /* 命令前台运行中退出 */
            // 设置 shell 所在进程组为前台进程组
            if (give_terminal_to(shell_pgid) != 0) {
                fprintf(stderr, "Error: fail to set fg.\n");
                longjmp(jmpbuf, 122);
            }
    }
}

/*
 * 处理 SIGINT 信号
 */
static void sigint_handler(int signo) {
    printf("bye bye~\n");
    longjmp(jmpbuf, 1);
}

/*
 * 处理 SIGWINCH 信号
 */
static void sigwinch_handler(int signo) {
    sigwinch_received = 1;
}

/*
 * 将 后台进程组 设为前台进程组
 */
int give_terminal_to(pid_t pgrp) {
    sigset_t set, oset;

    sigemptyset(&set);
    sigaddset(&set, SIGTTOU);
    sigaddset(&set, SIGTTIN);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGCHLD);
    sigemptyset(&oset);
    sigprocmask(SIG_BLOCK, &set, &oset);

    if (tcsetpgrp(shell_fd, pgrp) < 0) {
        perror("set fg");
        sigprocmask(SIG_SETMASK, &oset, (sigset_t *) NULL);
        return -1;
    } else
        sigprocmask(SIG_SETMASK, &oset, (sigset_t *) NULL);

    return 0;
}

/*
 * 将指定文件描述符移至进程中可用fd的最高位置
 */
static int move_to_high_fd(int fd) {
    int script_fd, nfds, ignore;

    nfds = getdtablesize();
    if (nfds <= 0)
        nfds = 20;
    if (nfds > 256)
        nfds = 256;

    for (nfds--; nfds > 3; nfds--)
        if (fcntl(nfds, F_GETFD, &ignore) == -1)
            break;

    if (nfds && fd != nfds && (script_fd = dup2(fd, nfds)) != -1)
        return script_fd;

    return fd;
}

/*
 * tab 补全
 * 注: 只有命令行的第一个单词才使用自定义的 tab 补全策略, 后面的使用本地文件夹内的文件名补全
 */
char **shell_completion(const char *text, int start, int end) {
    char **matches = NULL;

    // 命令行的第一个单词才自定义补全策略
    if (start == 0)
        matches = rl_completion_matches(text, command_generator);

    return (matches);
}

/*
 * tab 补全: 生成每个命令
 */
char *command_generator(const char *text, int state) {
    static char **words;
    static int words_index;

    if (state == 0) {
        words = trie_starts_with(p_trie, text);
        words_index = 0;
    }
    if (words != NULL && words[words_index] != NULL) {
        char *p_return = words[words_index++];
        if (words[words_index] == NULL)
            free(words);
        return p_return;
    }
    return NULL;
}
