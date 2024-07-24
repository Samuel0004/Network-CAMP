#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <pthread.h>

#define BUF_SIZE 4096

void error_handling(char *message);

void* Send_File(void* sockfd);

typedef struct packet_t{
	char message[BUF_SIZE];
	int message_size;
}packet_t;


pthread_t id;

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	struct timeval timeout;
	fd_set reads, cpy_reads;
    
    socklen_t adr_sz;
	int fd_max, str_len, fd_num, i;
    
    char buf[BUF_SIZE];
    
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));
    
    bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
    listen(serv_sock, 5);
    
    FD_ZERO(&reads);
	FD_SET(serv_sock, &reads);
	fd_max = serv_sock;
    
    
	while (1)
	{
		cpy_reads = reads;
		timeout.tv_sec = 5;
		timeout.tv_usec = 5000;

		if ((fd_num = select(fd_max+1, &cpy_reads, 0, 0, &timeout)) == -1)
			break;
		
		if (fd_num == 0)
			continue;

		for (i = 0; i < fd_max + 1; i++)
		{
			if (FD_ISSET(i, &cpy_reads))
			{
				if (i == serv_sock)     // connection request!
				{
					adr_sz = sizeof(clnt_adr);
					clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);

					FD_SET(clnt_sock, &reads);
					
                    if (fd_max < clnt_sock)
						fd_max = clnt_sock;
					
                    printf("connected client: %d \n", clnt_sock);
                    
                    int arg = clnt_sock;
                    pthread_create(&id, NULL, Send_File, &arg);
                    
                    pthread_join(id, NULL);
				}
				else    // read message!
				{
					str_len = read(i, buf, BUF_SIZE);
					if (str_len == 0)    // close request!
					{
						FD_CLR(i, &reads);
						close(i);
						printf("closed client: %d \n", i);
					}
                    else
					{
						printf("Message From Client: %s\n", buf);
					}
				}
			}
		}
	}
	close(serv_sock);
    return 0;
}
void error_handling(char *message){
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

void* Send_File(void* clnt_sock){
    int sockfd = *((int*)clnt_sock);
    char buf[BUF_SIZE];

    FILE * fp = fopen("pexels-souvenirpixels-417074.jpg", "rb"); 

    int read_cnt = 0;   

    while(1)
    {
        packet_t *file_to_send = (packet_t*)malloc(sizeof(packet_t));
        memset(file_to_send, 0, sizeof(packet_t));

        read_cnt = fread((void*)buf, 1, BUF_SIZE, fp);

        memcpy(file_to_send->message, buf, read_cnt);
        file_to_send->message_size = read_cnt;

        //printf("Message Size: %d\n", file_to_send->message_size);
        write(sockfd, buf, read_cnt);

        if(read_cnt < BUF_SIZE)
            break;
        
    }
        
    // if not shutdown, read() in client side does not end, which disables code to move forward even if ended sending file
    shutdown(sockfd, SHUT_WR);

    fclose(fp);
}