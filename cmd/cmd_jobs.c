//
// Created by iwant on 19-5-29.
//

/* =====================================================================
 * 本 Shell 提供作业控制功能，支持的命令有：jobs fg bg。
 * 说明：
 *    1）jobs
 *       Usage: jobs [-rs]
 *       作用: 查看后台运行作业的状态。
 *       选项: 其中，选项 r 列出所有正在运行中的; 选项 s 列出所有正停止的
 *    2）fg
 *       Usage: fg 作业标号
 *       作用: 将指定后台作业切换至前台运行
 *    3）bg
 *       Usage: bg 作业标号
 *       作用: 继续在后台运行已停止的作业
 *    4）其他
 *       可以通过 ctrl + Z 组合键将正在执行前台作业暂停并置于后台
 * =====================================================================
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wait.h>

#include "../shell.h"

typedef struct {
    unsigned int job_id;        /* 作业标号 */
    pid_t pgid;                 /* 后台进程组长 */
    job_state job_state;        /* 作业状态 */
    int status;                 /* 信号编号 or 退出状态 */
    char *cmd;                  /* 命令 */
} job;

// shell 的pgid --> shell.c
extern pid_t shell_pgid;
// 暂存前台进程运行的命令 --> shell.c
extern char cmd[MAX_CMD_LEN];

// 所有作业
static job jobs[MAX_JOB_LEN];

static void print_job(job *p_job);

/*
 * 释放 job
 */
static void free_job(job *job) {
    if (job != NULL) {
        job->job_id = 0;
        if (job->cmd != NULL)
            free(job->cmd);
    }
}

/*
 * 处理后台执行
 */
void process_backstage_execution(char **words) {
    // 去掉所有的 “&”
    char **p = words;
    char **curr_p = words;
    while (*p != NULL) {
        if (strcmp(BACKGROUND_SYM, *p) == 0)
            ++p;
        else
            *curr_p++ = *p++;
    }
    *curr_p = NULL;
}

/*
 * 向作业队列中添加新的工作
 *
 * @return 成功时返回 0; 失败时，-1表示无堆空间，-2表示作业队列已慢
 */
int add_job(pid_t pgid, job_state state, char *p_cmd) {
    for (unsigned int i = 0; i < MAX_JOB_LEN; ++i) {
        job *p_job = &jobs[i];
        if (p_job->job_id == 0) {
            p_job->job_id = i + 1;
            p_job->pgid = pgid;
            p_job->job_state = state;
            p_job->status = 0;
            p_job->cmd = malloc(strlen(p_cmd) + 1);
            if (p_job->cmd == NULL) {
                fprintf(stderr, "no space!\n");
                p_job->job_id = 0;
                p_job->pgid = 0;
                return -1;
            }
            strcpy(p_job->cmd, p_cmd);
            print_job(p_job);
            return 0;
        }
    }
    fprintf(stderr, "Job queue is full!\n");
    return -2;
}

/*
 * 扫描作业队列，若发生状态转变则输出
 */
void scan_jobs() {
    for (int i = 0; i < MAX_JOB_LEN; ++i) {
        int status;
        job *p_job = &jobs[i];
        if (p_job->job_id != 0) {
            int ret = waitpid(p_job->pgid, &status, WNOHANG | WCONTINUED | WUNTRACED);
            if (ret != 0 && ret != -1) {
                if (WIFEXITED(status)) {
                    // 正常退出
                    p_job->job_state = NORMAL_END;
                    p_job->status = WEXITSTATUS(status);
                    print_job(p_job);
                    free_job(p_job);
                } else if (WIFSIGNALED(status)) {
                    // 异常退出
                    p_job->job_state = ABNORMAL_END;
                    p_job->status = WTERMSIG(status);
                    print_job(p_job);
                    free_job(p_job);
                } else if (WIFSTOPPED(status)) {
                    // 暂停状态
                    p_job->job_state = STOP;
                    p_job->status = WSTOPSIG(status);
                    print_job(p_job);
                } else if (WIFCONTINUED(status)) {
                    // 暂停后已经继续的
                    p_job->job_state = STOP_CONTINUE;
                    print_job(p_job);
                    p_job->job_state = RUNNING;
                }
            }
        }
    }
}

/*
 * 打印一个作业的状态
 */
static void print_job(job *p_job) {
    size_t index = 0;
    char str[32];
    if (p_job->job_state == RUNNING)
        printf("%-5d%-8d%-32s%s\n", p_job->job_id, p_job->pgid, "正运行", p_job->cmd);
    else if (p_job->job_state == STOP) {
        strcpy(str, "已停止");
        index = strlen("已停止");
        str[index++] = '(';
        strcpy(str + index, strsignal(p_job->status));
        index += strlen(strsignal(p_job->status));
        str[index++] = ')';
        str[index] = '\0';
        printf("%-5d%-8d%-32s%s\n", p_job->job_id, p_job->pgid, str, p_job->cmd);
    } else if (p_job->job_state == STOP_CONTINUE)
        printf("%-5d%-8d%-32s%s\n", p_job->job_id, p_job->pgid, "已继续", p_job->cmd);
    else if (p_job->job_state == ABNORMAL_END) {
        strcpy(str, "异常退出");
        index = strlen("异常退出");
        str[index++] = '(';
        index += sprintf(str + index, "%d", p_job->status);
        str[index++] = ')';
        str[index] = '\0';
        printf("%-5d%-8d%-32s%s\n", p_job->job_id, p_job->pgid, str, p_job->cmd);
    } else if (p_job->job_state == NORMAL_END) {
        strcpy(str, "已完成");
        index = strlen("已完成");
        str[index++] = '(';
        index += sprintf(str + index, "%d", p_job->status);
        str[index++] = ')';
        str[index] = '\0';
        printf("%-5d%-8d%-32s%s\n", p_job->job_id, p_job->pgid, str, p_job->cmd);
    }
}

static void jobs_help() {
    fprintf(stderr, "Usage: jobs [-rs]\n");
}

/*
 * jobs 命令
 * 支持：-r（仅输出正在运行的） -s（仅输出停止的）
 */
void exec_jobs(char *words[]) {
    char **p = words + 1;
    if (*p == NULL) {
        // 列出所有的 jobs
        for (int i = 0; i < MAX_JOB_LEN; ++i)
            if (jobs[i].job_id != 0) {
                print_job(&jobs[i]);
                if (jobs[i].job_state == ABNORMAL_END || jobs[i].job_state == NORMAL_END)
                    free_job(&jobs[i]);
            }
    } else if (strcmp("-r", *p) == 0 && *(p + 1) == NULL) {
        // 支持 -r 只输出正在运行的
        for (int i = 0; i < MAX_JOB_LEN; ++i) {
            job *p_job = &jobs[i];
            if (p_job->job_id != 0 && p_job->job_state == RUNNING)
                print_job(p_job);
        }
    } else if (strcmp("-s", *p) == 0 && *(p + 1) == NULL) {
        // 支持 -s 只输出停止的
        for (int i = 0; i < MAX_JOB_LEN; ++i) {
            job *p_job = &jobs[i];
            if (p_job->job_id != 0 && p_job->job_state == STOP)
                print_job(p_job);
        }
    } else
        // help
        jobs_help();
}

static void fg_help() {
    fprintf(stderr, "Usage: fg 作业标号\n");
}

/*
 * fg 命令，用于恢复指定标号的作业恢复至前台
 * 说明：fg 标号
 *
 * @return 成功时返回值0
 */
int exec_fg(char *words[]) {
    char **p = words + 1;
    if (*p == NULL) {
        fg_help();
        return -1;
    }
    long num = strtoul(*p, NULL, 10);
    if (num <= 0 || num > MAX_JOB_LEN) {
        fprintf(stderr, "Error: Invalid job label.\n");
        fg_help();
        return -2;
    }
    job *p_job = &jobs[num - 1];
    if (p_job->job_id == 0 || p_job->job_state == ABNORMAL_END || p_job->job_state == NORMAL_END) {
        fprintf(stderr, "Error: job's status error.\n");
        fg_help();
        return -3;
    }
    // 输出名称
    printf("%s continue!\n", p_job->cmd);

    // 执行下面一些逻辑时屏蔽一切信号（尤其是 SIGCHLD ），执行完毕后再恢复
    sigset_t set, oset;
    sigfillset(&set);
    sigemptyset(&oset);
    sigprocmask(SIG_BLOCK, &set, &oset);

    // 将该进程组设为前台进程组
    if (give_terminal_to(p_job->pgid) != 0) {
        fprintf(stderr, "Error: fail to set fg.\n");
        return -5;
    }
    int pgid = shell_pgid;
    // 如果处在 STOP 状态，发送 SIGCONT 信号
    if (p_job->job_state == STOP) {
        if (killpg(p_job->pgid, SIGCONT) != 0) {
            give_terminal_to(pgid);
            perror("fail to send signal");
            return -6;
        }
    }
    // 更新 cmd
    strcpy(cmd, p_job->cmd);
    // 若上面都成功，则从 jobs 中去除该 job
    free_job(p_job);

    // 恢复信号
    sigprocmask(SIG_SETMASK, &oset, (sigset_t *) NULL);
    return 0;
}


static void bg_help() {
    fprintf(stderr, "Usage: bg 作业标号\n");
}

/*
 * bg 命令，用于继续执行后台已停止的进程
 *
 * 说明：bg 标号
 */
void exec_bg(char *words[]) {
    char **p = words + 1;
    if (*p == NULL) {
        bg_help();
        return;
    }
    long num = strtoul(*p, NULL, 10);
    if (num <= 0 || num > MAX_JOB_LEN) {
        fprintf(stderr, "Error: Invalid job label.\n");
        bg_help();
        return;
    }
    if (jobs[num - 1].job_id == 0 || jobs[num - 1].job_state != STOP) {
        fprintf(stderr, "Error: job's status error.\n");
        bg_help();
        return;
    }
    job *p_job = &jobs[num - 1];
    // 输出名称
    printf("%s &\n", p_job->cmd);

    // 执行下面一些逻辑时屏蔽一切信号（尤其是 SIGCHLD ），执行完毕后再恢复
    sigset_t set, oset;
    sigfillset(&set);
    sigemptyset(&oset);
    sigprocmask(SIG_BLOCK, &set, &oset);

    // 发送 SIGCONT 信号
    if (killpg(p_job->pgid, SIGCONT) != 0) {
        give_terminal_to(p_job->pgid);
        perror("fail to send signal");
        return;
    }
    // 若上面都成功，修改该 job 的状态
    p_job->status = RUNNING;

    // 恢复信号
    sigprocmask(SIG_SETMASK, &oset, (sigset_t *) NULL);
}


