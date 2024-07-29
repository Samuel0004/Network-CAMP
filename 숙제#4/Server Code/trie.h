#ifndef TRIE_H
#define TRIE_H

#define N 27

typedef struct TrieNode TrieNode;

typedef struct word_t {
    char word[64];
    int search_count;
} word_t;

typedef struct found_t {
    char word[64];
    TrieNode *root;
} found_t;

typedef struct TrieNode {
    char data;
    TrieNode* children[N];
    int is_leaf;
    int is_complete;
} TrieNode;

TrieNode* make_trienode(char data);
void free_trienode(TrieNode* node);
TrieNode* insert_trie(TrieNode* root, char* word, int count);
void Find_Match_Prefix_Node(TrieNode* root, char* search_word, char* word, found_t* found_array, int* index, int nthMatch);
void Find_Complete_InChild(TrieNode* root, char* word, word_t* word_array, int* index);
int Get_Complete_Words(word_t* word_array, found_t* found_array, int* index);

#endif // TRIE_H