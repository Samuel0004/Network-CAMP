#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <termios.h>
#include <ctype.h>
#include <fcntl.h>

#define BUFSIZE 100
#define COLOR_GREEN "\x1b[32m"
#define COLOR_MANGENTA "\x1b[33m"
#define COLOR_RESET "\x1b[0m"
#define CLEAR_LINES "\033[0J"

char FILLER[64] = {"\x1b[32mSearch Word:\x1b[0m "};

typedef struct word_t
{
    char word[64];
    int search_count;
} word_t;

struct termios saved_attributes;

int search_word_len = 0;
word_t word_t_array[10];

void error_handling(char *message);
int sock;
void *send_search_word(void *arg);
void *recv_search_result(void *arg);
int Receive_Count();
void Receive_Rearch_Result(int file_count);
void Print_Result();
void set_input_mode();
void reset_input_mode();
char *toLower(char *s);

char word_in_search[BUFSIZE];

int main(int argc, char *argv[])
{
    struct sockaddr_in server_address;
    pthread_t snd_thread, rcv_thread;

    if (argc != 3)
    {
        printf("usage %s <Server IP> <Server Port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);

    if (sock == -1)
    {
        error_handling("socket() error");
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(argv[1]);
    server_address.sin_port = htons(atoi(argv[2]));

    // After Connect,
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        error_handling("connect() error");
    }

    set_input_mode();

    printf(CLEAR_LINES);
    printf("\033[H\033[2J"); // Move to top left and clear the console
    fflush(stdout);
    write(0, FILLER, strlen(FILLER)); //Print "Search Word:" in GREEN 

    pthread_create(&snd_thread, NULL, send_search_word, (void *)&sock);
    pthread_create(&rcv_thread, NULL, recv_search_result, (void *)&sock);
    pthread_join(snd_thread, NULL);
    pthread_join(rcv_thread, NULL);

  
    reset_input_mode();
    close(sock);
    return 0;
}

void *send_search_word(void *arg) // send thread main
{
    int ch = 0;
    int idx = -1;
    int sock = *((int *)arg);
    
    
    
    while (read(0, &ch, 1) > 0)
    {
        if (ch == 4)// ctrl + d 
        {
            if (idx == -1)
                exit(0);
            else
                continue;
        }
        else if (ch == 127) // backspace
        {   
            //only backspace when there is a letter
            if (idx >= 0)   
            {
                word_in_search[idx--] = '\0';
                write(0, "\b \b", 3);
            }
        }
        else if (ch == '\n'){
            break;
        }
        else                // character input
        {
            word_in_search[++idx] = ch;
            write(0, &ch, sizeof(int));
        }
        ch = 0;
        search_word_len = strlen(word_in_search);
        
        if(search_word_len<=0)
            Print_Result(0);
        else
            write(sock, word_in_search, strlen(word_in_search));
    }

    //reset_input_mode();
    return NULL;
}

void *recv_search_result(void *arg) // read thread main
{
    int sock = *((int *)arg);

    int str_len;
    while (1)
    {
        // Receive Count
        int count = Receive_Count();

        if(count>0)
            Receive_Rearch_Result(count);

        Print_Result(count);
    }
    return NULL;
}

void Print_Result(int count)
{
    printf("\033[H");       // Move origin
    printf("\033[1B");      // Move two lines down 
    printf(CLEAR_LINES);    // Clear clear old search result
    printf("-----------------------------\n");
   
    // Make search engine case-insensitive
    char word_in_search_lower[BUFSIZE];
    strcpy(word_in_search_lower, word_in_search);
    toLower(word_in_search_lower);

    for (int i = 0; i < count; i++) {
        char word_lower[BUFSIZE];
        strcpy(word_lower, word_t_array[i].word);
        toLower(word_lower);

        // Find the position where the search word is located
        char *pos = strstr(word_lower, word_in_search_lower);

        // IF match
        if (pos) {
            // Print the part of word before match
            fwrite(word_t_array[i].word, 1, pos - word_lower, stdout);
            // Print the matching part with color
            printf("%s%.*s%s", COLOR_MANGENTA, (int)strlen(word_in_search), word_t_array[i].word + (pos - word_lower), COLOR_RESET);
            // Print the part of word after match
            printf("%s\n", word_t_array[i].word + (pos - word_lower) + strlen(word_in_search));
        } else {
            // No match, print the word normally
            //printf("%s\n", word_t_array[i].word);
        }
    }
    
    printf("\033[H");  // Move origin again
    printf("\033[%dC", 13+search_word_len); // Move cursor to end of current searching word 
    fflush(stdout);

}

void Receive_Rearch_Result(int word_count)
{
    char message[BUFSIZE];

    // Clear Array
    memset(word_t_array, 0, sizeof(word_t_array));

    word_t *received_struct = (word_t *)malloc(sizeof(word_t));

    int index = 0;
    // Read Files and Add to Array
    while (read(sock, received_struct, sizeof(word_t)) != 0)
    {
        strcpy(word_t_array[index].word, received_struct->word);

        index++;
        if (index >= word_count)
            break;
    }
}

int Receive_Count()
{
    int index;
    // Read 4byte worth of file_count
    if (read(sock, &index, sizeof(index)) == -1)
    {
        error_handling("Receive_File_Count() error");
    }
    return index;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void set_input_mode()
{
    struct termios new_attribute;

    // Save Original Attributes into global structure
    tcgetattr(STDIN_FILENO, &saved_attributes);
    atexit(reset_input_mode); // call reset_input_mode function when program exits

    // Get Current Terminal Attribute and save to new attribute
    tcgetattr(STDIN_FILENO, &new_attribute);

    // Change Terminal
    new_attribute.c_lflag &= ~(ICANON | ECHO); // Clear ICANON and ECHO mode (Non cannon + )
    new_attribute.c_cc[VMIN] = 1;              // Set minimum number of characters for read
    new_attribute.c_cc[VTIME] = 0;             // Set timeout (decisecond) for read
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_attribute);
}

void reset_input_mode()
{
    // Restore the original attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
}

char *toLower(char *s)
{
    for (char *p = s; *p; p++)
        *p = tolower(*p);
    return s;
}