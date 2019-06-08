//
// Created by iwant on 19-6-6.
//

#include <ctype.h>

#include "str_op.h"

int get_word_num(char *chars) {
    int num = 0;
    char *p = chars;
    while (*p != '\0' && isspace(*p))
        p++;
    if (*p == '\0')
        return num;
    while (*p != '\0') {
        if (!isspace(*p)) {
            ++num;
            while (*p != '\0' && !isspace(*p))
                ++p;
        }
        if (*p == '\0')
            break;
        p++;
    }
    return num;
}

void split_str(char *chars, char *words[]) {
    int i = 0;
    char *p = chars;
    while (isspace(*p))
        ++p;
    while (*p != '\0') {
        if (!isspace(*p)) {
            words[i++] = p;
            while (*p != '\0' && !isspace(*p))
                ++p;
        }
        if (*p == '\0')
            break;
        else
            *p = '\0';
        ++p;
    }
}

void clean_char_in_word(char c, char *word) {
    char *p1 = word;
    char *p2 = word;
    while (*p1 != '\0') {
        if (*p1 == c)
            ++p1;
        else
            *p2++ = *p1++;
    }
    *p2 = *p1;
}
