#ifndef TSHELL_PREFIX_TREE
#define TSHELL_PREFIX_TREE

#define MAX_DEEP 64

/*
 * 前缀树节点
 * 支持的字符: a-z(0-25) A-Z(26-51) 0-9(52-61)
 */
typedef struct trie_node {
    struct trie_node *next_nodes[62];   /* 指向下一层 */
    char is_word;                       /* 是否是单词 */
    char c;                             /* 当前字符 */
    char align[2];                      /* 对齐 */
} node;

typedef struct {
    node *heads[62];
} trie;

/*
 * 创建前缀树
 */
trie *trie_create();

/*
 * 插入
 *
 * @result 插入失败时,返回 -1; 成功是,返回 0.
 */
int trie_insert(trie *p_tree, const char *word);

/*
 * 搜索指定单词是否在树中
 */
int trie_search(trie *p_tree, const char *word);

/*
 * 查找出树中以给定单词为前缀的所有单词
 */
char **trie_starts_with(trie *p_tree, const char *prefix);

/*
 * 释放前缀树
 */
void trie_free(trie *p_tree);


#endif //TSHELL_PREFIX_TREE

