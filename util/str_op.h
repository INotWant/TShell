//
// Created by iwant on 19-6-6.
//

#ifndef TSHELL_STR_OP_H
#define TSHELL_STR_OP_H

/*
 * 计算字符串中 “词” 的个数
 */
int get_word_num(char *chars);

/*
 * 以空白字符分割字符串
 */
void split_str(char *chars, char *words[]);

/*
 * 从字符串中清理指定字符
 */
void clean_char_in_word(char, char *);


#endif //TSHELL_STR_OP_H
