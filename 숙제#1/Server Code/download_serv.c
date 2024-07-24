#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>

void error_handling(char *message);

#define FILE_COUNT 4
#define BUFSIZE 4096

// Typedefine structure of file (contain name and file size)
typedef struct file_t{
    char file_name[64];
    long file_size;
}file_t; //alias as file_t

file_t file_t_array[FILE_COUNT];

int GetClientRequestIndex(int sock);

void Send_File(FILE *fp, int sock);

int CreateArrayFromDirectory(const char *dirname);

int main(int argc, char* argv[]){

    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;

    socklen_t clnt_addr_size;
    
    int file_count = CreateArrayFromDirectory("./");

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock ==-1){
        error_handling("socket() error");
    }
    
    int option = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if((bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    clnt_addr_size = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
    if(clnt_sock== -1)
        error_handling("accpet() error");

    // Send Number of files
    write(clnt_sock, &file_count, sizeof(file_count));

    for(int i=0;i<file_count;i++){
        write(clnt_sock, &file_t_array[i], sizeof(file_t));
    }

    char buffer[BUFSIZE];
    
    while(1){
        
        int request_index = GetClientRequestIndex(clnt_sock);
        int read_cnt=0;
        int file_len =0;
        FILE *fp;
        fp = fopen(file_t_array[request_index].file_name, "rb");
        
        Send_File(fp, clnt_sock);
        
        fclose(fp);
    }

    return 0;
}

void error_handling(char *message){
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

int GetClientRequestIndex(int clnt_sock) {
    int index;
    if (read(clnt_sock, &index, sizeof(index)) == -1) {
        error_handling("GetClientRequestIndex() error");
    }
    return index;
}

int CreateArrayFromDirectory(const char *dirname){
    DIR* dir;
    struct dirent* entry;
    FILE* open_file;
    long file_size;
    dir = opendir(dirname);

    int count = 0;

    if(!dir)
        error_handling("opendir() error");

    while((entry = readdir(dir))!= NULL){
        if(strcmp(entry->d_name,".") ==0 || strcmp(entry->d_name,"..")==0)
            continue;

        if(entry->d_type == DT_REG){
            open_file = fopen(entry->d_name,"r");
            
            fseek(open_file,0,SEEK_END);

            file_size = ftell(open_file);

            strcpy(file_t_array[count].file_name,entry->d_name);
            file_t_array[count].file_size = file_size;

            count++;
            fclose(open_file);
        }
    }
    closedir(dir);

    return count;
}

void Send_File(FILE *fp, int sockfd) {
    char buffer[BUFSIZE];
    
    while (1) {
        size_t bytes_read = fread(buffer, 1, BUFSIZE, fp);
        if (bytes_read > 0) {
            send(sockfd, buffer, bytes_read, 0);
        }
        if (bytes_read < BUFSIZE) {
            if (ferror(fp)) {
                error_handling("fread() error");
            }
            break;
        }
    }
}