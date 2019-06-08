//
// Created by iwant on 19-5-23.
//

/* ===========================================================
 * export 作为本 Shell 的内部命令，提供一些操作环境变量的功能。
 * 用法：export export [-gn] [名称[=值] ...] 或 export -p
 *
 * 说明：
 *    export -g [名称 ...]   查询指定名称对应的值，一次可查询多个
 *    export -d [名称 ...]   删除指定名称的环境变量，一次可删除多个
 *    export -p              显示所有环境变量
 *    export [名称[=值] ...]  添加环境变量
 * ===========================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern char **environ;

static void export_help() {
    fprintf(stderr, "Usage: export [-gd] [名称[=值] ...] 或 export -p\n");
}

/*
 * 根据指定字符切分字符串
 * 注意：直接在原字符串上修改
 */
static void split_str(char *chars, char c, char *a_chars[], int len) {
    int i = 0;
    char *p = chars;
    if (*p != '\0') {
        a_chars[i++] = p;
        ++p;
    }
    if (len == i)
        return;
    while (*p != '\0') {
        if (*p == c) {
            *p = '\0';
            if (*(p + 1) != '\0' && *(p + 1) != c) {
                a_chars[i++] = p + 1;
                if (len == i)
                    return;
            }
        }
        ++p;
    }
}

/*
 * export 命令
 */
void exec_export(char *words[]) {
    if (words[1] == NULL)
        export_help();
    else if (strcmp(words[1], "-p") == 0) {
        // 显示所有环境变量
        char **p = environ;
        while (*p != NULL) {
            printf("%s\n", *p);
            ++p;
        }
    } else if (strcmp(words[1], "-d") == 0) {
        // 删除指定的变量
        for (int i = 2; words[i] != NULL; ++i)
            if (unsetenv(words[i]) != 0)
                fprintf(stderr, "unset env fail: %s\n", words[i]);
    } else if (strcmp(words[1], "-g") == 0) {
        // 查询环境变量
        if (words[2] == NULL)
            export_help();
        char **p = words + 2;
        while (*p != NULL) {
            char *tp = getenv(*p);
            if (tp != NULL)
                printf("%s=%s\n", *p, tp);
            ++p;
        }
    } else if (words[1][0] == '-') {
        export_help();
    } else {
        // 设置环境变量
        char **p = words + 1;
        while (*p != NULL) {
            char *name_value_chars[2];
            name_value_chars[0] = *p;
            name_value_chars[1] = NULL;
            split_str(*p, '=', name_value_chars, 2);
            if (name_value_chars[1] == NULL || setenv(name_value_chars[0], name_value_chars[1], 1) != 0)
                fprintf(stderr, "set env fail: %s\n", *p);
            ++p;
        }
    }
}

