#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024
#define MAX_CLNT 256

void error_handling(char *message);
void *handle_clnt(void * arg);

typedef struct packet_t{
	char message[BUF_SIZE];
	int message_size;
}packet_t;


pthread_mutex_t mutx;
int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];


int main(int argc, char *argv[])
{
    int serv_sd, clnt_sd;
    char buf[BUF_SIZE];
    
    int read_cnt;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;

    pthread_t t_id;
    pthread_mutex_init(&mutx, NULL);
    
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    
    serv_sd = socket(PF_INET, SOCK_STREAM, 0);
    
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));
    
    bind(serv_sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
    listen(serv_sd, 5);
    
    while(1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sd = accept(serv_sd, (struct sockaddr*)&clnt_adr,&clnt_adr_sz);
        
        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sd;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sd);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    
    close(serv_sd);
    return 0;
}

void error_handling(char *message){
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

void *handle_clnt(void * arg)
{
    FILE* fp;
    int clnt_sock = *((int*)arg);
    int str_len = 0, i;
    char msg[BUF_SIZE];
    int read_cnt = 0;

    fp = fopen("high.jpg", "rb"); 
    
    while(1)
    {
        packet_t *file_to_send = (packet_t*)malloc(sizeof(packet_t));
        memset(file_to_send, 0, sizeof(packet_t));

        read_cnt = fread((void*)msg, 1, BUF_SIZE, fp);

        memcpy(file_to_send->message, msg, read_cnt);
        file_to_send->message_size = read_cnt;

        //printf("Message Size: %d\n", file_to_send->message_size);
        write(clnt_sock, msg, read_cnt);

        if(read_cnt < BUF_SIZE)
            break;
    }
    
    shutdown(clnt_sock, SHUT_WR);
    fclose(fp);

    pthread_mutex_lock(&mutx);

    for (i = 0; i < clnt_cnt; i++) // remove disconnected client
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i++ < clnt_cnt-1)
                clnt_socks[i] = clnt_socks[i+1];
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return NULL;
}
