#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/time.h>

#define BUF_SIZE 4096

typedef struct file_t{
    char file_name[BUF_SIZE];
    long file_size;
}file_t; //alias as file_t


typedef struct packet_t{
	int sequence;
	char message[BUF_SIZE];
	int message_size;
}packet_t;

void error_handling(char *message);
void timeout(int sig);
void Wait_For_ACK();
void Send_Packet();
int Send_File_Info();

packet_t *current_file;
int sock;
int pkt_seq = 0;
struct sockaddr_in serv_adr;

char file_name[BUF_SIZE] = "test.jpg";

int main(int argc, char *argv[])
{
	char message[BUF_SIZE];
	int str_len;
	socklen_t adr_sz;
	int file_size;

	struct sigaction act;
	act.sa_handler = timeout;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGALRM, &act, 0);

	if (argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}
	
	sock = socket(PF_INET, SOCK_DGRAM, 0);   
	if (sock == -1)
		error_handling("socket() error");
	
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));
	
	
	// Open file and Send File info to Receiver
	file_size = Send_File_Info();

	printf("%d",file_size);

	// Lets remove this for now
	//Wait_For_ACK(sock);

	// Send Actual files
	int read_len = 0;
	
	FILE *fp = fopen(file_name,"rb");

	while (read_len < file_size)
	{
		// Read some from file 
		int bytes_read = fread(message,1,BUF_SIZE, fp);

		read_len += bytes_read;

		// Sender Sends 
		if(bytes_read>0){
			// Create Structure to hold file and packet sequence
			packet_t *file_to_send = (packet_t*)malloc(sizeof(packet_t));
			memset(file_to_send, 0, sizeof(packet_t));

			//message[bytes_read] = 0;

			printf("Bytes: %d\n",bytes_read);
			printf("SEQ: %d\n",pkt_seq);

			memcpy(file_to_send->message, message, bytes_read);
			file_to_send->sequence = pkt_seq;
			file_to_send->message_size = bytes_read;
			
			// Save for later resend
			current_file = file_to_send;

			// Send File and Start a timer (10 seconds)
			Send_Packet(file_to_send);
		}
		if(bytes_read < BUF_SIZE){
			break;
		}

		// If receive ACK, continue loop
		Wait_For_ACK(sock);

	}	

	fclose(fp);
	close(sock);
	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

void timeout(int sig){
	// When timeout occur, Escape out of recvfrom() and call SendFile() again
	printf("TIMEOUT\n");
}

void Wait_For_ACK(){
	// WAIT for ACK from receiver

	struct sockaddr_in from_adr;
	socklen_t adr_sz;
	int ack_seq;

	adr_sz = sizeof(from_adr);

	// Block until receive packet
	while(recvfrom(sock, &ack_seq, sizeof(ack_seq), 0, (struct sockaddr*)&from_adr, &adr_sz)==-1){
		Send_Packet(current_file);
	}
	
	//printf("ACK Seq: %d",ack_seq);
	
	// Should receive same sequence from receiver
	if(ack_seq == pkt_seq){
		// Stop alarm when get get seq
		alarm(0);
		pkt_seq++;
	}
	else{
		printf("DROP wrong packet\n");
	}	
}

void Send_Packet(packet_t *file_to_send){
	// Send structure 
	//printf("SENT %s\n", file_to_send->message);
	sendto(sock, file_to_send, sizeof(packet_t), 0, (struct sockaddr*)&serv_adr, sizeof(serv_adr));

	// Start Timer
	alarm(2);
}

int Send_File_Info(){
	FILE* fp = fopen(file_name,"rb");
	int file_size;

	fseek(fp,0,SEEK_END);
	file_size = ftell(fp);

	file_t *file_info = (file_t*)malloc(sizeof(file_t));
	strcpy(file_info->file_name, file_name);
	file_info->file_size = file_size;

	// Send file info
	sendto(sock, file_info, sizeof(file_t), 0, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
	fclose(fp);

	return file_size;
}