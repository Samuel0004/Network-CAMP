#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#define FILE_COUNT 64   // Max File Count Per Client in Serv Array
#define BUFSIZE 4096
#define FILE_NAME 64    
#define MAX_PROGESS 20  // For Progress Bar

// Typedefine structure of file (contain name and file size)
typedef struct file_t{
    char file_name[FILE_NAME];
    long file_size;
    int is_directory; // 1 for directory 
}file_t; //alias as file_t

typedef struct request_t{
    int option;
    char request_text[FILE_NAME];
}request_t;

file_t file_t_array[FILE_COUNT];
int file_count;
int sock;

void Request_Directory();
void error_handling(char *message);
int Receive_File_Count();
void Receive_Files(int file_count);
void Print_Files();
void Do_Client_Request();
void *send_msg(void *arg);
void *recv_msg(void *arg);
void Send_Request(int option, char *request_text);
void Download_File(int index);
int Find_File_Index(char *file_name);
int Find_Dir_Index(char *file_name);
void Upload_File(char *file_name);

enum Option {
  DOWNLOAD,
  UPLOAD,
  MOVEDIR,
  SHOW
};

int main(int argc, char *argv[])
{
    struct sockaddr_in server_address;
    char buf[BUFSIZE];
    char directory[FILE_NAME];

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

    printf("Please Type One of the Following Commands:\n\n");
    printf("[Download]:   d <file_name>\n");
    printf("[Upload]:     u <file_name>\n");
    printf("[Move]:       m <dir_name>\n");
    printf("[Refresh]:    r\n");
    printf("[EXIT]:       q\n");

    int request_type =0;
    while(1){
        printf("\n--------------REMOTE SIMPLE SHELL--------------\n");
        // Get Directory 
        Request_Directory();

        // Get Request through Keyboard and Send to Server
        Do_Client_Request();
    }

    close(sock);
    return 0;
}

void Do_Client_Request(){
    char msg[BUFSIZE];
    char option;
    char file_name[FILE_NAME];


    printf("=>");

    fgets(msg, BUFSIZE, stdin);
    
    if (!strcmp(msg, "q\n"))
    {
        close(sock);
        exit(0);
    }

    if(!strcmp(msg, "r\n"))
        return;

    
    if (sscanf(msg, " %c %s", &option, file_name) != 2) {
        printf("Invalid command format. Please enter a proper command.\n");
        return;    
    }

    int file_index;

    switch (option) {
        case 'd':
            // Handle download
            printf("Download command received for file: %s\n", file_name);

            file_index = Find_File_Index(file_name);
            if(file_index == -1){
                printf("No such file Exists in Current Directory\n");
                return;
            }

            // Send Download Request to Server and Download File
            Send_Request(DOWNLOAD, file_name);
            Download_File(file_index);

            break;
        case 'u':
            // Handle upload
            printf("Upload command received for file: %s\n", file_name);

            // Send Upload Request and then Send File 
            Send_Request(UPLOAD, file_name);
            Upload_File(file_name);

            break;
        case 'm':
            // Handle move
            printf("Move command received for directory: %s\n", file_name);

            file_index = Find_Dir_Index(file_name);
            if(file_index == -1){
                printf("No such Directory Exists in Current Directory\n");
                return;
            }

            Send_Request(MOVEDIR, file_name);

            break;
        default:
            printf("Enter a proper command.\n");
            break;
    }
    
    return;
}

int Find_File_Index(char *file_name){
    for(int i=0;i<file_count;i++){
        if(!strcmp(file_t_array[i].file_name, file_name) && file_t_array[i].is_directory != 1)   
            return i;
    }
    return -1;
}

int Find_Dir_Index(char *file_name){
    for(int i=0;i<file_count;i++){
        if(!strcmp(file_t_array[i].file_name, file_name) && file_t_array[i].is_directory != 0)   
            return i;
    }
    return -1;
}

void Upload_File(char *file_name){
    
    FILE* open_file;
    long file_size;
    char buffer[BUFSIZE];

    open_file = fopen(file_name,"rb");
    if (!open_file) {
        return;  
    }

    // Get size of file
    fseek(open_file,0,SEEK_END);
    file_size = ftell(open_file);
    fseek(open_file, 0, SEEK_SET);

    // First send the file_t structure
    file_t *file_to_send = (file_t*)malloc(sizeof(file_t));
    strcpy(file_to_send->file_name,file_name);
    file_to_send->file_size = file_size;
    file_to_send->is_directory = 0;

    write(sock, file_to_send, sizeof(file_t));
    long file_len =0;
    // Then Send Actual File 
    while (1) {
        size_t bytes_read = fread(buffer, 1, BUFSIZE, open_file);

        file_len += bytes_read;

        printf("Uploading [");
        int progress = (file_len * MAX_PROGESS) / file_size;

        for (int i=0;i<MAX_PROGESS;i++) {
            if(i<progress)
                printf ("#");
            else
                printf(" ");
		}
        printf("] %03d%% (%ld/%ldBytes)\r", (int)((file_len * 100) / file_size) ,file_len, file_size);
        
		fflush(stdout);
		// sleep(1);		

        if (bytes_read > 0) {
            send(sock, buffer, bytes_read, 0);
        }
        if (bytes_read < BUFSIZE) {
            if (ferror(open_file)) {
                error_handling("fread() error");
            }
            printf("\x1b[K"); 
            printf("UPLOAD COMPLETE\n");
            break;
        }
    }

    fclose(open_file);
    
    sleep(2);
    return;
}


void Download_File(int file_index){

    FILE *fp;
    char buf[BUFSIZE];
    int read_cnt = 0;
    long file_len = 0;

    // Open a file with chosen file name to write
    fp = fopen(file_t_array[file_index].file_name, "wb");

    while ((read_cnt = read(sock, buf, BUFSIZE)) != 0)
    {
        if (read_cnt == -1)
            error_handling("read() error");

        file_len += read_cnt;

        fwrite((void *)buf, 1, read_cnt, fp);
        

        printf("Downloading [");
        int progress = (file_len * MAX_PROGESS) / file_t_array[file_index].file_size;
        for (int i=0;i<MAX_PROGESS;i++) {
            if(i<progress)
                printf ("#");
            else
                printf(" ");
		}
        printf("] %03d%% (%ld/%ldBytes)\r", (int)((file_len * 100) / file_t_array[file_index].file_size) ,file_len, file_t_array[file_index].file_size);
        
		fflush(stdout);
		//sleep(1);		


        if (file_len >= file_t_array[file_index].file_size){
            printf("\x1b[K"); 
            printf("DOWNLOAD COMPLETE\n");
            break;
        }
    }
    fclose(fp);
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void Send_Request(int option, char *request_text){
    // Createa Request Struct with Passed ARguments
    request_t *request = (request_t*)malloc(sizeof(request_t));
    request->option = option;
    strcpy(request->request_text, request_text);
    
    // Send to Server
    write(sock, request, sizeof(request_t));
}

void Receive_Files(int file_count)
{
    char message[BUFSIZE];

    // Clear Array
    memset(file_t_array, 0, sizeof(file_t_array));

    file_t *received_struct = (file_t *)malloc(sizeof(file_t));

    int index = 0;
    // Read Files and Add to Array
    while (read(sock, received_struct, sizeof(file_t)) != 0)
    {
        strcpy(file_t_array[index].file_name, received_struct->file_name);
        file_t_array[index].file_size = received_struct->file_size;
        file_t_array[index].is_directory = received_struct->is_directory;

        index++;
        if (index >= file_count)
            break;
    }
}

void Print_Files(int file_count)
{
    printf("\nFiles from Server Directory:\n\n");
    for (int i = 0; i < file_count; i++)
    {
        if(file_t_array[i].is_directory)
            printf("[%d] %-30s DIR\n", i, file_t_array[i].file_name);
        else
            printf("[%d] %-30s %ld bytes\n", i, file_t_array[i].file_name, file_t_array[i].file_size);
    }
    printf("\n");
}

int Receive_File_Count()
{
    int index;
    // Read 4byte worth of file_count
    if (read(sock, &index, sizeof(index)) == -1)
    {
        error_handling("Receive_File_Count() error");
    }
    return index;
}

void Request_Directory()
{
    int count;
    Send_Request(SHOW, "");
    
    count = Receive_File_Count(sock); // Get How Many Files to Receive (later decies when to stop read loop)
    file_count = count;
    Receive_Files(count); // Get Files and Save them to Array
    // Show Directory Entries
    Print_Files(count);
}

