#ifndef TSHELL_SHELL_H
#define TSHELL_SHELL_H

#include "util/sem_op.h"
#include "util/str_op.h"
#include "util/prefix_tree.h"

#define PATH_NAME   "TShell"
#define PROJ_ID     0x0001

#define MAX_CMD_LEN 128                     /* 命令行可接受的最大字节数 */

#define CMD_SYM "CMD_SYM"                   /* 修改命令提示的环境变量名称 */
#define MAX_CMD_SYM_LEN 64                  /* 命令提示符支持的最大字节数 */
#define DEFAULT_CMD_SYM "-> "               /* 默认命令提示符 */

#define REDIRECT_INPUT_SYM  "<"             /* 标准输入重定向标识符 */
#define REDIRECT_OUTPUT_SYM ">"             /* 标准输出(含标准错误输出)重定向标识符 */
#define REDIRECT_OUTPUT_APPEND_SYM ">>"     /* 追加标准输出(含标准错误输出)重定向标识符 */

#define MAX_JOB_LEN 1024                    /* 可控制的最大作业数 */
#define BACKGROUND_SYM "&"                  /* 后台运行标识符 */
// 作业状态
typedef enum {
    RUNNING,            /* 正运行 */
    STOP,               /* 已停止 */
    STOP_CONTINUE,      /* 已继续 */
    ABNORMAL_END,       /* 异常结束 */
    NORMAL_END,         /* 已完成 */
} job_state;

int give_terminal_to(pid_t pgrp);

void exec_export(char *words[]);

int process_input_redirect(char *words[]);

int process_output_redirect(char *words[]);

int process_output_append_redirect(char *words[]);

void process_backstage_execution(char **words);

int add_job(pid_t pgid, job_state state, char *p_cmd);

void scan_jobs();

void exec_jobs(char *words[]);

int exec_fg(char *words[]);

void exec_bg(char *words[]);

#endif //TSHELL_SHELL_H
