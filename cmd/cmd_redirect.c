//
// Created by iwant on 19-5-24.
//

/* =====================================================================
 * '>' '<' '>>' 为本 shell 的重定向标识符。
 * 说明：
 *    1）重定向标识符说明
 *      '>'   用作输出重定向（默认为标准输出）
 *      '<'   用作输入重定向
 *      '>>'  用作追加重定向（默认为标准输出追加）
 *    2）本 shell 中重定向标识符前的 '0' '1' '2' 有特殊含义：
 *      '0'   标准输入
 *      '1'   标准输出
 *      '2'   标准错误
 *    3）本 shell 中重定向标识符后的 "&0" "&1" "&2" 有特殊含义：
 *      "&0"    引用 fd0
 *      "&1"    引用 fd1
 *      "&2"    引用 fd2
 *    4）注意：在使用重定向时需要用空白字符划分，这是与标准 shell 不同的
 *      `ls > a 2 > &1` -->  正确
 *      `ls >a`         -->  错误，此处把 `>a` 作为一个文件
 *      `ls > a 2>&1`   -->  错误， `2>&1` 应写为 `2 > &1`（标准错误重定向）
 *    [注] 该设计造成 0 1 2 做命令参数时需要加 '' 单引号:
 *       ls '2' > a.txt -->  2 不表示标准错误重定向，而作为 ls 的一个参数
 * =====================================================================
 */

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "../shell.h"

#define REF_INPUT   "&0"
#define REF_OUTPUT  "&1"
#define REF_ERR     "&2"

extern int fd_redirect_in;
extern int fd_redirect_out;
extern int fd_redirect_err;

/*
 * 处理命令字符字符
 * @return 成功时返回 fd，返回负数表示出错。
 */
static int process_words(char *word, char *words[]) {
    char **p = words;
    while (strcmp(*p, word) != 0)
        ++p;
    if (*(p + 1) == NULL) {
        fprintf(stderr, "Redirect needs to specify the file!");
        return -1;
    }
    /*
     * 1）考虑相对路径 or 绝对路径问题
     * 2）考虑打开的是不是文件的问题
     * 3）考虑文件不存在时创建的问题
     * 4）文件权限等问题
     * [x] open 帮助考虑了以上问题
     * 注：考虑何时关闭 fd
     * 1）子进程关闭时自动关闭
     * 2）父进程的在子进程启动完毕后关闭
     */
    int fd = -1;
    if (strcmp(*(p + 1), REF_INPUT) == 0)       /* 处理 &0 */
        fd = fd_redirect_in;
    else if (strcmp(*(p + 1), REF_OUTPUT) == 0) /* 处理 &1 */
        fd = fd_redirect_out;
    else if (strcmp(*(p + 1), REF_ERR) == 0)    /* 处理 &2 */
        fd = fd_redirect_err;
    else {
        if (strcmp(word, REDIRECT_INPUT_SYM) == 0)
            fd = open(*(p + 1), O_RDONLY);
        else if (strcmp(word, REDIRECT_OUTPUT_SYM) == 0)
            fd = open(*(p + 1), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        else if (strcmp(word, REDIRECT_OUTPUT_APPEND_SYM) == 0)
            fd = open(*(p + 1), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    }
    if (fd < 0)
        perror("Open file fail when redirect");
    else {
        // 去掉与重定向相关的字符
        char **new_p = p + 2;
        // 如果是输出重定向，则留下重定向标识符(即 > 或 >>)，下面还要使用
        if (strcmp(word, REDIRECT_OUTPUT_SYM) == 0 || strcmp(word, REDIRECT_OUTPUT_APPEND_SYM) == 0)
            ++p;
        while (*new_p != NULL)
            *p++ = *new_p++;
        *p = *new_p;
    }
    return fd;
}

/*
 * 处理输入重定向
 * @return 成功时返回相应的文件描述符（大于 0），否则失败
 */
int process_input_redirect(char *words[]) {
    return process_words(REDIRECT_INPUT_SYM, words);
}

/*
 * 修改 标准输出重定向 or 标准错误重定向
 */
static void modify_redirect_out_or_err(int fd, char *word, char *words[]) {
    char **p = words;
    while (strcmp(*p, word) != 0)
        ++p;
    char **new_p = p + 1;
    if (p != words) {
        --p;
        if (strcmp(*p, "1") == 0) {
            fd_redirect_out = fd;
            dup2(fd_redirect_out, STDOUT_FILENO);
            new_p = p + 2;
        } else if (strcmp(*p, "2") == 0) {
            fd_redirect_err = fd;
            dup2(fd_redirect_err, STDERR_FILENO);
            new_p = p + 2;
        } else {
            fd_redirect_out = fd;
            dup2(fd_redirect_out, STDOUT_FILENO);
            ++p;
            new_p = p + 1;
        }
    } else{
        fd_redirect_out = fd;
        dup2(fd_redirect_out, STDOUT_FILENO);
    }
    // 清理重定向相关标识符
    while (*new_p != NULL)
        *p++ = *new_p++;
    *p = *new_p;
}

/*
 * 处理输出重定向(截断)
 * @return 成功时返回相应的文件描述符（大于 0），否则失败
 */
int process_output_redirect(char *words[]) {
    int fd = process_words(REDIRECT_OUTPUT_SYM, words);
    if (fd >= 0)
        modify_redirect_out_or_err(fd, REDIRECT_OUTPUT_SYM, words);
    return fd;
}

/*
 * 处理输出重定向(追加)
 * @return 成功时返回相应的文件描述符（大于 0），否则失败
 */
int process_output_append_redirect(char *words[]) {
    int fd = process_words(REDIRECT_OUTPUT_APPEND_SYM, words);
    if (fd >= 0)
        modify_redirect_out_or_err(fd, REDIRECT_OUTPUT_APPEND_SYM, words);
    return fd;
}