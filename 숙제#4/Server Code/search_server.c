#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <pthread.h>
#include "trie.h"

#define BUFSIZE 100
#define WORD_NUM 100
#define MAX_CLNT 256

word_t word_array[WORD_NUM];
int array_index = 0;

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

TrieNode *root_node;

int Initialize_Socket(struct sockaddr_in serv_adr, int port_number);
void ReadData_MakeTrie(TrieNode *root);
word_t FindWordInArray(int index);
void SearchEngine(char *search_word, int client_sock);
void Sort_Array(word_t *array, int count);
void *handle_clnt(void *arg);

char *toLower(char *s);
void error_handling(char *message);
int main(int argc, char *argv[])

{
    pthread_t t_id;
    socklen_t clnt_adr_sz;
    struct sockaddr_in serv_adr, clnt_adr;
    int serv_sock, clnt_sock;
    pthread_mutex_init(&mutx, NULL);

    root_node = make_trienode('\0');

    // Make Trie Structure
    ReadData_MakeTrie(root_node);

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Initialize Socket
    serv_sock = Initialize_Socket(serv_adr, atoi(argv[1]));
    int option = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));


    while (1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }

    free_trienode(root_node);
    return 0;
}

void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);
    int str_len = 0, i;
    char msg[BUFSIZE];

    // Search Words and Send to Client
    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0){
        msg[str_len] = '\0';
        printf("Received Word: %s\n",msg);
        SearchEngine(msg,clnt_sock);
    }

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++) // remove disconnected client
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i++ < clnt_cnt - 1)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx);

    close(clnt_sock);
    printf("Client Disconnected\n");
    return NULL;
}

void SearchEngine(char *search_word, int client_sock)
{
    char word[64];
    found_t found_array[100];
    int index = 0;
    char client_request[64];

    // Search Trie Node with lower case
    strcpy(client_request, search_word);
    toLower(client_request);

    // Get All Nodes that match Prefex
    Find_Match_Prefix_Node(root_node, client_request, word, found_array, &index, 0);

    // Save Pseudo Information into temp
    word_t temp_array[100];
    int count = Get_Complete_Words(temp_array, found_array, &index);

    word_t array_to_send[count];
    // With pseudo info, find word in Real array
    for (int i = 0; i < count; i++)
    {
        word_t found_word = FindWordInArray(temp_array[i].search_count);
        array_to_send[i] = found_word;
        // printf("Word: %s Index: %d\n",found_word.word, found_word.search_count);
    }

    Sort_Array(array_to_send, count);

    if (count > 10)
        count = 10;

    
    // First Send Count
    write(client_sock, &count, sizeof(count));

    // Then Send MAX 10 Most Searched Words
    for (int i = 0; i < count; i++)
    {
        write(client_sock, &array_to_send[i], sizeof(array_to_send[i]));
        printf("Word: %s Index: %d\n", array_to_send[i].word, array_to_send[i].search_count);
    }

    printf("\n");
}

void Sort_Array(word_t *array, int count)
{
    // Use Bubble Sort to Sort Array in Decreasing Order
    word_t temp;
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = 0; j < count - 1 - i; j++)
        {
            if (array[j].search_count < array[j + 1].search_count)
            {
                temp = array[j];
                array[j] = array[j + 1];
                array[j + 1] = temp;
            }
        }
    }
}

word_t FindWordInArray(int index)
{
    return word_array[index];
}

void ReadData_MakeTrie(TrieNode *root)
{
    FILE *fp = fopen("data.txt", "rb");
    char *line = NULL;
    long len = 0;
    int search_count = 0;
    char word[64];

    // For each Line
    while ((getline(&line, &len, fp)) != -1)
    {
        word_t *word_struct = (word_t *)malloc(sizeof(word_t));

        // Get Data from read line
        sscanf(line, "%[a-zA-Z ] %d", word, &search_count);
        word[strlen(word) - 1] = '\0';

        // Make a word_t and save the original word and search count to Word Array
        strcpy(word_struct->word, word);
        word_struct->search_count = search_count;
        word_array[array_index] = *(word_struct);

        // Make Root out of pseudo word and pseudo index
        // Word is lower cased and search count is replaced by index in real word array
        root = insert_trie(root, toLower(word), array_index);
        array_index++;
    }

    fclose(fp);
}

char *toLower(char *s)
{
    for (char *p = s; *p; p++)
        *p = tolower(*p);
    return s;
}


int Initialize_Socket(struct sockaddr_in serv_adr, int port_number){
    int serv_sock;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    int option = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(port_number);
    
    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr))==-1)
        error_handling("bind() error");
    
    if(listen(serv_sock, 5)==-1)
        error_handling("listen() error");
    
    return serv_sock;
}
void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
