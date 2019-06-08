//
// Created by iwant on 19-6-7.
//

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <stdio.h>

#include "prefix_tree.h"

trie *trie_create() {
    trie *p_trie = malloc(sizeof(trie));
    if (!p_trie)
        return NULL;
    memset(p_trie, 0, sizeof(trie));
    return p_trie;
}

// 获取字符对应的位置
static int get_char_pos(char c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a';
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    return -1;
}

int trie_insert(trie *p_tire, const char *word) {
    char c;
    int i = 0;
    node **nodes = p_tire->heads;
    while ((c = word[i]) != '\0') {
        int pos = get_char_pos(c);
        if (pos < 0)
            return -1;
        if (nodes[pos] == NULL) {
            nodes[pos] = malloc(sizeof(node));
            if (!nodes[pos])
                return -1;
            memset(nodes[pos], 0, sizeof(node));
            nodes[pos]->c = c;
        }
        if (word[i + 1] == '\0')
            nodes[pos]->is_word = 1;
        nodes = nodes[pos]->next_nodes;
        ++i;
    }
    return 0;
}

int trie_search(trie *p_tire, const char *word) {
    char c;
    node **nodes = p_tire->heads;
    int i = 0;
    while ((c = word[i]) != '\0') {
        int pos = get_char_pos(c);
        if (pos < 0)
            return -1;
        if (nodes[pos] == NULL)
            return -1;
        if (word[i + 1] == '\0')
            return nodes[pos]->is_word;
        nodes = nodes[pos]->next_nodes;
        ++i;
    }
    return 0;
}

static char prefix_arr[MAX_DEEP];
static sigjmp_buf jmpbuf;
static char **match_words;
static int match_words_pos;
static int match_words_len;

static void dfs_error_free() {
    int i = 0;
    while (match_words[i] != NULL) {
        free(match_words[i]);
        ++i;
    }
    free(match_words);
}

static void dfs(node *p_node, int prefix_len) {
    if (p_node != NULL) {
        if (p_node->is_word) {
            if (match_words_pos >= match_words_len) {
                match_words_len *= 2;
                char **new_match_words = realloc(match_words, sizeof(char *) * match_words_len);
                if (!new_match_words) {
                    dfs_error_free();
                    longjmp(jmpbuf, 1);
                }
                match_words = new_match_words;
                memset(match_words + match_words_pos, 0, match_words_len / 2 * sizeof(char *));
            }
            char *p = malloc(prefix_len + 2);
            if (!p) {
                dfs_error_free();
                longjmp(jmpbuf, 1);
            }
            memcpy(p, prefix_arr, prefix_len);
            p[prefix_len] = p_node->c;
            p[prefix_len + 1] = '\0';
            match_words[match_words_pos++] = p;
        }
        prefix_arr[prefix_len++] = p_node->c;
        for (int i = 0; i < 62; ++i)
            if (p_node->next_nodes[i])
                dfs(p_node->next_nodes[i], prefix_len);
    }
}

char **trie_starts_with(trie *p_tire, const char *prefix) {
    if (prefix == NULL || *prefix == '\0')
        return NULL;

    node **nodes = p_tire->heads;
    char c;
    int i = 0;
    node *end_node = NULL;

    while ((c = prefix[i]) != '\0') {
        int pos = get_char_pos(c);
        if (pos < 0 || nodes[pos] == NULL)
            return NULL;
        if (prefix[i + 1] == '\0')
            end_node = nodes[pos];
        nodes = nodes[pos]->next_nodes;
        ++i;
    }

    if (end_node == NULL)
        return NULL;

    match_words_pos = 0;
    match_words_len = 32;

    match_words = malloc(sizeof(char *) * match_words_len);
    if (!match_words)
        return NULL;
    memset(match_words, 0, sizeof(char *) * match_words_len);

    if (setjmp(jmpbuf) != 0)
        return NULL;

    memcpy(prefix_arr, prefix, strlen(prefix) - 1);

    dfs(end_node, strlen(prefix) - 1);

    return match_words;
}

static void nodes_free(node **nodes) {
    for (int i = 0; i < 62; ++i) {
        if (nodes[i]) {
            nodes_free(nodes[i]->next_nodes);
            free(nodes[i]);
        }
    }
}

void trie_free(trie *p_tire) {
    nodes_free(p_tire->heads);
    free(p_tire);
}

