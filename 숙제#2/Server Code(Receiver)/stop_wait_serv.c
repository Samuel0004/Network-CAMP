#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 4096
void error_handling(char *message);

typedef struct file_t{
    char file_name[BUF_SIZE];
    long file_size;
}file_t; //alias as file_t


typedef struct packet_t{
	int sequence;
	char message[BUF_SIZE];
	int message_size;
}packet_t;

int main(int argc, char *argv[])
{
	int serv_sock;
	char file_name[BUF_SIZE];
	long file_size;
	int str_len;
	socklen_t clnt_adr_sz;

	struct sockaddr_in serv_adr, clnt_adr;
	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	
	serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (serv_sock == -1)
		error_handling("UDP socket creation error");
	
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));
	
	if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");

	int ack_seq = 0;

	file_t *file_info = (file_t*)malloc(sizeof(file_t));
	clnt_adr_sz = sizeof(clnt_adr);

	recvfrom(serv_sock, file_info, sizeof(packet_t), 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

	strcpy(file_name, file_info->file_name);
	file_size = file_info->file_size;

	printf("File Name: %s\n",file_name);
	printf("File Size: %ld\n",file_size);

	//sendto(serv_sock, &ack_seq, sizeof(ack_seq), 0, (struct sockaddr*)&clnt_adr, clnt_adr_sz);
	//ack_seq++;
	
	
	FILE* fp= fopen(file_name, "wb");

	int read_len = 0;

	while(read_len < file_size) 
	{	
		packet_t *recv_file = (packet_t*)malloc(sizeof(packet_t));

		char message[BUF_SIZE];
		int seq;

		// Receive a packet from client 
		if((recvfrom(serv_sock, recv_file, sizeof(packet_t), 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz))==-1){
			error_handling("recvfrom() error");
		}
		
		// Extract info from structure
		int message_len = recv_file->message_size;
		printf("Message Length: %d\n",message_len);
		memcpy(message,recv_file->message, message_len);
		seq = recv_file->sequence;
	
		if(seq == ack_seq){

			fwrite((void*)message, 1, message_len, fp);
			printf("SEQ: %d\n",seq);
			
			// Send ACK (which is saved seq)
			sendto(serv_sock, &seq, sizeof(seq), 0, (struct sockaddr*)&clnt_adr, clnt_adr_sz);
			read_len += message_len;
			ack_seq++;
		}

	}

	close(serv_sock);
	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
