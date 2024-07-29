#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trie.h"


TrieNode* make_trienode(char data) {
    // Allocate memory for a TrieNode
    TrieNode* node = (TrieNode*) calloc (1, sizeof(TrieNode));
    for (int i=0; i<N; i++)
        node->children[i] = NULL;
    node->is_leaf = 0;
    node->data = data;
    return node;
}

void free_trienode(TrieNode* node) {
    // Free the trienode sequence
    for(int i=0; i<N; i++) {
        if (node->children[i] != NULL) {
            free_trienode(node->children[i]);
        }
        else {
            continue;
        }
    }
    free(node);
}

TrieNode* insert_trie(TrieNode* root, char* word, int count) {
    // Inserts the word onto the Trie
    // ASSUMPTION: The word only has lower case characters
    TrieNode* temp = root;

    for (int i=0; word[i] != '\0'; i++) {
        // Get the relative position in the alphabet list
        int idx;
        if(word[i] == ' '){
            idx = 26;
        }
        else{
            idx = (int) word[i] - 'a';
        }
        if (temp->children[idx] == NULL) {
            // If the corresponding child doesn't exist,
            // simply create that child!
            temp->children[idx] = make_trienode(word[i]);
        }
        else {
            // Do nothing. The node already exists
        }
        // Go down a level, to the child referenced by idx
        // since we have a prefix match
        temp = temp->children[idx];
    }
    // At the end of the word, mark this node as the leaf node
    temp->is_leaf = count;
    temp->is_complete = 1;
    return root;
}


void Find_Match_Prefix_Node(TrieNode* root, char* search_word, char* word, found_t* found_array, int* index, int nthMatch) {
    if (!root) return;

    TrieNode* temp = root;
    char toStr[2];
    toStr[1] = '\0';
    toStr[0] = temp->data;

    // Create a new string for this recursive call
    char newWord[strlen(word) + 2]; // +1 for the new character and +1 for the null terminator
    strcpy(newWord, word);
    strcat(newWord, toStr);

    // If this character matches the nth word 
    if (search_word[nthMatch] == temp->data) {
        nthMatch++;
        
        // If this is the last character in search word
        if (nthMatch == strlen(search_word)) {
            // Add to list and end search in this cycle
            found_array[*index].root = temp;
            strcpy(found_array[*index].word, newWord);

            (*index)++;
            //printf("Word at last match is %s\n", newWord);
            return;
        }
    }

    // If there are more characters to find for, recursively search for them 
    for (int i = 0; i < N; i++) {
        if (temp->children[i]) {
            Find_Match_Prefix_Node(temp->children[i], search_word, newWord, found_array, index, nthMatch);
        }
    }
}

void Find_Complete_InChild(TrieNode* root, char* word, word_t* word_array, int* index) {
    if (!root) return;
    
    TrieNode* temp = root;

    // If this is complete word, add to word array 
    if (temp->is_complete) {
        strcpy(word_array[*index].word, word);
        word_array[*index].search_count = temp->is_leaf;
        (*index)++;
        //printf("Complete Word: %s\n",word);
        //printf("%d\n",*(index));
    }

    // There can be more complate Words, So Traverse further down the tree
    for (int j = 0; j < N; j++) {
        if (temp->children[j]) {
            char toStr[2];
            toStr[1] = '\0';
            toStr[0] = temp->children[j]->data;

            char newWord[strlen(word) + 2]; // +1 for the new character and +1 for the null terminator
            strcpy(newWord, word);
            strcat(newWord, toStr);

            Find_Complete_InChild(temp->children[j], newWord, word_array, index);
           
        }
    }
}

int Get_Complete_Words( word_t* word_array,  found_t* found_array, int* index){
    int word_array_index = 0;
    for(int i=0;i<*index;i++){
        Find_Complete_InChild(found_array[i].root, found_array[i].word, word_array, &word_array_index);
    }
    return word_array_index;
}

