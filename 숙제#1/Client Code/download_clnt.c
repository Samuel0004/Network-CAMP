#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define FILE_COUNT 4
#define BUFSIZE 4096

// Typedefine structure of file (contain name and file size)
typedef struct file_t{
    char file_name[64];
    long file_size;
}file_t; //alias as file_t

file_t file_t_array[FILE_COUNT];

void error_handling(char *message);
int Receive_File_Count(int socket);
void Receive_Files(int sock,int file_count);
void Print_Files();
int Get_File_Index();

int main(int argc, char* argv[]){

    int sock;
    struct sockaddr_in server_address;
    char buf[BUFSIZE];
    int file_count;

    if(argc!=3){
        printf("usage %s <Server IP> <Server Port>",argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);

    if(sock ==-1){
        error_handling("socket() error");
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(argv[1]);
    server_address.sin_port = htons(atoi(argv[2]));

    if(connect(sock, (struct sockaddr*)&server_address, sizeof(server_address))==-1){
        error_handling("connect() error");
    }

    // Get How Many Files to Receive (later decies when to stop read loop)
    file_count = Receive_File_Count(sock);

    // Get Files and Save them to Array
    Receive_Files(sock, file_count);

    FILE *fp;
    int read_cnt=0;
    int file_len=0;
    int chosen_index;

    while(1){
        file_len = 0;

        // Show Previously Saved File data
        Print_Files(file_count);

        // Get Which Index to send to Server to Download
        chosen_index = Get_File_Index();

        if(chosen_index == -1)
            return 0;

        // Send Index to Server
        if (write(sock, &chosen_index, sizeof(chosen_index)) == -1) {
            error_handling("write() error");
        }

        // Open a file with chosen file name to write
        fp = fopen(file_t_array[chosen_index].file_name, "wb");

        while((read_cnt = read(sock, buf, BUFSIZE))!=0){
            if(read_cnt==-1)
                error_handling("read() error");

            file_len+=read_cnt;

            fwrite((void*)buf, 1, read_cnt, fp);

            if(file_len>=file_t_array[chosen_index].file_size)
                break;
        }
        
        //buf[file_len] = 0; // Caused an issue with GetFileIndex
        fclose(fp);
    }

    close(sock);
    return 0;
}

void error_handling(char *message){
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

void Receive_Files(int sock, int file_count){
    char message[BUFSIZE];

    file_t *received_struct;
    received_struct = (file_t*)malloc(sizeof(file_t));
    
    int index = 0;
    while (read(sock, received_struct, sizeof(file_t)) != 0){

        strcpy(file_t_array[index].file_name,received_struct->file_name);
        file_t_array[index].file_size = received_struct->file_size;

        index++;
        if(index>=file_count)
            break;
    }
}


int Get_File_Index() {
    int index;
    printf("Enter the file index to download: ");
    scanf("%d", &index);

    return index;
}

void Print_Files(int file_count){
    //printf("-------------------------------\n");
    //printf("       FILE DOWNLOADER         \n");
    //printf("-------------------------------\n");

    printf("\nFiles from Server Directory:\n\n");
    for(int i=0;i<file_count;i++){
        printf("[%d] File Name: %s File Size: %ld\n", i, file_t_array[i].file_name,file_t_array[i].file_size);
    }
}

int Receive_File_Count(int sock){
    
    int index;

    // Read 4byte worth of file_count
    if(read(sock, &index, sizeof(index))==-1){
        error_handling("Receive_File_Count() error");
    }
   
    return index;
}

