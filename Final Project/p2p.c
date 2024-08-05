#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>

#define BUFSIZE 1024
#define MAX_PROGESS 20 // For Progress Bar

#pragma region Structure Declaration
typedef struct receiver_t
{
    int nth_receiver;
    int segment_from_receiver_cnt;
    struct sockaddr_in receiver_adr;
} receiver_t;

typedef struct file_info_t
{
    char file_name[64];
    long total_file_size;
    int segment_count;
    long segment_size;
    int max_receiver_cnt;
} file_info_t;

typedef struct segment_t
{
    long sement_size;
    char *segment;
    int nthSegment;
} segment_t; // alias as file_t

#pragma endregion

#pragma region Method Declaration
// Thread Methods
void *conn_receiver(void *arg);
void *accpt_receiver(void *arg);
void *handle_clnt(void *arg);
void *accpt_snd_seg(void *arg);
void *accpt_rcv_seg(void *arg);
void *flood_segment(void *arg);

// Thread Initalizing Method
void Make_Connection_W_Receivers_ThreadInit();
void Receive_Segment_From_Sender_ThreadInit();

// Helper Methods
void error_handling(char *msg);
int Initialize_Socket(int port_number);
void GetOptArgs(int argc, char *argv[]);
void Close_Sockets();
int Find_Nth_Receiver();

// Sender Method
void Prepare_File_Via_Seg();
void Print_Sender_Progress(int count, int peer_num, long sent_len, float elapsed_time);
void Wait_Receiver_Connection();
void Send_Info();

// Receiver Method
void Print_Receiver_Progress(int count, int peer_num, long sent_len, float elapsed_time);
void Connect_To_Sender();
void Receive_Info();
void Send_Segment();
void Wait_All_Segment();

#pragma endregion

#pragma region Global Variables 
int current_receiver_cnt = 0;
int receiver_MAX = 0;
int finished_segment_cnt = 0;

// Arrays to track socket/receiver info/segment
int *recv_sock_array;
file_info_t *file_info;
receiver_t *receiver_array;
receiver_t this_receiver;
segment_t *segment_array;

// Track two specific sockets/port_num separately from receiver info
int this_sock, sender_sock;
int this_port_num, sender_port_num;
struct sockaddr_in this_adr, sender_adr;

// File/Segment Values
int segment_size;
int current_segment_cnt = 0;
char sender_address[64];
char file_name[64];

// Others
float total_time = 0;
int rflag, sflag;
long accum_sent_len = 0;

// Pthread methods
pthread_mutex_t mutx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_finish = PTHREAD_COND_INITIALIZER;

#pragma endregion 


// Main
int main(int argc, char *argv[])
{
    pthread_t t_id;

    if (argc < 2)
    {
        printf("Please start by entering -r for receiver mode / -s for sender mode\n");
        exit(1);
    }

    GetOptArgs(argc, argv);

    this_sock = socket(PF_INET, SOCK_STREAM, 0);

    // Initialize this socket
    this_sock = Initialize_Socket(this_port_num);

    printf("\033[H\033[J");

    if (rflag == 1)
    {
        // Receiver Code

        // Connect to Sender and send Info to Connect to this receiver
        Connect_To_Sender();

        // Receive Information from Sender
        Receive_Info();

        // Make Thread To receive Segment from Sender
        Receive_Segment_From_Sender_ThreadInit();

        // Make Thread To Connection with other Receiver
        Make_Connection_W_Receivers_ThreadInit();

        // Wait for All Segments to be Received
        Wait_All_Segment();
    }
    else if (sflag == 1)
    {
        // Sender Code

        // Segment the Predefined info and make a struct
        Prepare_File_Via_Seg();

        // Accept Receiver Connections until MAX
        Wait_Receiver_Connection();

        // Send Receiver info to all Receivers
        Send_Info();

        // Send Segment to Receivers
        Send_Segment();

        // Sender uses no thread, so end when finish sending segment
        Close_Sockets();
    }

    return 0;
}


#pragma region Thread Initializing Methods

void Receive_Segment_From_Sender_ThreadInit()
{
    // Make thread to listen to Sender's File Sending
    pthread_t accpt_snd_seg_thread;
    pthread_create(&accpt_snd_seg_thread, NULL, accpt_snd_seg, NULL);
    pthread_detach(accpt_snd_seg_thread);
}

void Make_Connection_W_Receivers_ThreadInit()
{
    // Use thread to Accept connection from Receivers
    pthread_t conn_thread, accpt_thread;

    // Create two threads
    pthread_create(&accpt_thread, NULL, accpt_receiver, NULL);
    pthread_create(&conn_thread, NULL, conn_receiver, NULL);
    pthread_join(accpt_thread, NULL);
    pthread_join(conn_thread, NULL);

    return;
}

#pragma endregion

#pragma region Sender Methods

void Prepare_File_Via_Seg()
{
    FILE *file;
    long file_size;

    file_info = (file_info_t *)malloc(sizeof(file_info_t));

    file = fopen(file_name, "rb");
    if (!file)
    {
        printf("No such file in current Directory\n");
        exit(1);
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fclose(file);

    strcpy(file_info->file_name, file_name);
    file_info->total_file_size = file_size;
    file_info->segment_count = (file_size + segment_size - 1) / segment_size;
    file_info->max_receiver_cnt = receiver_MAX;
    file_info->segment_size = segment_size;

    return;
}

void Send_Segment()
{
    // Server sends Segment
    FILE *file;
    char buffer[segment_size];
    clock_t start, end;
    // Open file
    file = fopen(file_name, "rb");
    if (!file)
    {
        printf("No such file in current Directory\n");
        exit(1);
    }

    int rr_index = 0;
    int nth_segment = 0;
    while (1)
    {
        // Read file up to segment size
        int bytes_read = fread(buffer, 1, segment_size, file);

        // If read something,
        if (bytes_read > 0)
        {
            start = clock();

            // Send after wrapping up in segment_t structure
            segment_t *file_segment = (segment_t *)malloc(sizeof(segment_t));
            file_segment->nthSegment = nth_segment;
            file_segment->sement_size = bytes_read;
            file_segment->segment = (char *)malloc(sizeof(char) * (segment_size));
            memcpy(file_segment->segment, buffer, bytes_read);

            // Send the segment_t structure
            write(recv_sock_array[rr_index], file_segment, sizeof(segment_t));

            // Send the actual data pointed to by segment
            write(recv_sock_array[rr_index], file_segment->segment, file_segment->sement_size);

            //printf("Send %dth Segment to %d Receiver... %ld Bytes\n", nth_segment, rr_index, file_segment->sement_size);
            // Print Progress
            end = clock();

            float elapsed_time = ((float)(end - start)) / CLOCKS_PER_SEC;
            accum_sent_len += bytes_read;

            int sending = rr_index;
            Print_Sender_Progress(nth_segment,sending, bytes_read, elapsed_time);

            rr_index++;
            nth_segment++;

            // Apply Round Robin
            if (rr_index >= receiver_MAX)
            {
                rr_index = 0;
            }
        }
        else if (bytes_read == -1)
            error_handling("fread() error");

        // Break if this is last segment
        if (nth_segment >= file_info->segment_count)
        {
            break;
        }
    }
}

void Send_Info()
{
    // Sender Send File Info
    int i, j;

    for (i = 0; i < receiver_MAX; i++)
    {
        write(recv_sock_array[i], file_info, sizeof(file_info_t));
    }

    // Send How Many Segments to Wait for from Sender
    int each_segment_count = file_info->segment_count / receiver_MAX;
    int remainder = file_info->segment_count % receiver_MAX;
    int sent_byte_cnt = 0;

    for (i = 0; i < receiver_MAX; i++)
    {
        receiver_array[i].segment_from_receiver_cnt = each_segment_count;
    }

    for (i = 0; i < remainder; i++)
    {
        receiver_array[i].segment_from_receiver_cnt++;
    }

    // To Every Receiver
    for (i = 0; i < receiver_MAX; i++)
    {
        //  Send Every Receiver Information
        for (j = 0; j < receiver_MAX; j++)
        {
            write(recv_sock_array[i], &receiver_array[j], sizeof(receiver_t));
        }
    }

    return;
}

void Print_Sender_Progress(int count, int peer_num, long sent_len, float elapsed_time)
{
    total_time += elapsed_time;

    // Calculate Throughput in Mbps
    float throughput = (accum_sent_len / total_time) * 0.000008f;

    // Calcualte Progress
    int progress = (accum_sent_len * MAX_PROGESS) / file_info->total_file_size;
    printf("\033[K"); // Erase Progress Bar

    // Print Progress
    printf("Sending Peer [");
    for (int i = 0; i < MAX_PROGESS; i++)
    {
        if (i < progress)
            printf("#");
        else
            printf(" ");
    }
    printf("] %03d%% (%ld/%ldBytes) %.1fMbps (%.1fs)", (int)((accum_sent_len * 100) / file_info->total_file_size), accum_sent_len, file_info->total_file_size, throughput, total_time);

    // Move Down count lines (count is nth Segment) +1
    printf("\033[%dE", count + 1);

    // Print Receiving Information
    printf("  To Receiving Peer #%d : %.1f Mbps (%ld Bytes Sent / %.1f s)", peer_num, (sent_len / elapsed_time)  * 0.000008f, sent_len, elapsed_time);

    // Move Up again
    printf("\033[%dF", count + 1);
    fflush(stdout);

    // sleep(1);

    // If finish, move to end of line
    if (accum_sent_len >= file_info->total_file_size)
    {
        printf("\033[%dE", count + 2);
    }
}


void Wait_Receiver_Connection()
{
    // Wait for all receivers to connect
    int recv_sock;
    struct sockaddr_in recv_adr;
    int recv_adr_sz;

    while (1)
    {
        // Accept Receiver Connection
        recv_adr_sz = sizeof(recv_adr);
        recv_sock = accept(this_sock, (struct sockaddr *)&recv_adr, &recv_adr_sz);

        // Save its socket
        pthread_mutex_lock(&mutx);
        recv_sock_array[current_receiver_cnt] = recv_sock;
        pthread_mutex_unlock(&mutx);

        // Receive whick Socket should be used for other Receivers to Connect
        receiver_t *receiver_struct = (receiver_t *)malloc(sizeof(receiver_t));

        if (read(recv_sock, receiver_struct, sizeof(receiver_t)) == -1)
        {
            error_handling("Wait_Receiver_Connection() error");
        }

        // Fill in IP Address info (Receiver used INADDRANY, so need to write specific IP)
        receiver_struct->receiver_adr.sin_addr = recv_adr.sin_addr;
        // FIll in nth receiver information
        receiver_struct->nth_receiver = current_receiver_cnt;

        pthread_mutex_lock(&mutx);
        receiver_array[current_receiver_cnt] = *(receiver_struct);
        pthread_mutex_unlock(&mutx);

        if (++current_receiver_cnt >= receiver_MAX)
            break;
    }

    return;
}

#pragma endregion

#pragma region Receiver Methods


void Wait_All_Segment()
{
    // Wait for end of all receive and send of segments
    pthread_cond_wait(&cond_finish, &mutx);

    // Move to end of line
    printf("\033[%dE", current_segment_cnt+1);

    // Write file from received segments
    FILE *fp = fopen(file_info->file_name, "wb");

    for (int i = 0; i < current_segment_cnt; i++)
    {
        fwrite((void *)segment_array[i].segment, 1, segment_array[i].sement_size, fp);
    }
    fclose(fp);

    // Close All Sockets
    Close_Sockets();
    close(sender_sock);
}

void Print_Receiver_Progress(int count, int peer_num, long sent_len, float elapsed_time)
{
    total_time += elapsed_time;

    // Calculate Throughput in Mbps
    float throughput = (accum_sent_len / total_time) * 0.000008f;

    // Calcualte Progress
    int progress = (accum_sent_len * MAX_PROGESS) / file_info->total_file_size;
    printf("\033[K"); // Erase Progress Bar

    // Print Progress
    printf("Receiving Peer [");
    for (int i = 0; i < MAX_PROGESS; i++)
    {
        if (i < progress)
            printf("#");
        else
            printf(" ");
    }
    printf("] %03d%% (%ld/%ldBytes) %.1fMbps (%.1fs)", (int)((accum_sent_len * 100) / file_info->total_file_size), accum_sent_len, file_info->total_file_size, throughput, total_time);

    // Move Down count lines (count is nth Segment) +1
    printf("\033[%dE", count + 1);

    // Print Receiving Information
    if (peer_num == -1)
    {
        printf("  From Sending Peer : %.1f Mbps (%ld Bytes Sent / %.1f s)", (sent_len / elapsed_time)  * 0.000008f, sent_len, elapsed_time);
    }
    else
    {
        printf("  From Receiving Peer #%d : %.1f Mbps (%ld Bytes Sent / %.1f s)", peer_num, (sent_len / elapsed_time)  * 0.000008f, sent_len, elapsed_time);
    }

    // Move Up again
    printf("\033[%dF", count + 1);
    fflush(stdout);
}

void Receive_Info()
{
    // Receiver First Get File Info
    file_info = (file_info_t *)malloc(sizeof(file_info_t));

    if (read(sender_sock, file_info, sizeof(file_info_t)) == -1)
    {
        error_handling("Receive_Info() error");
    }

    // Get max receiver count -1 (for its own), so alocate memory for array
    segment_array = (segment_t *)malloc(sizeof(segment_t) * file_info->segment_count);
    receiver_MAX = (file_info->max_receiver_cnt) - 1;

    if (receiver_MAX < 0)
    {
        printf("No other Receivers\n");
        return;
    }

    receiver_array = (receiver_t *)malloc(sizeof(receiver_t) * receiver_MAX);
    recv_sock_array = (int *)malloc(sizeof(int) * receiver_MAX);

    receiver_t receive_struct;
    int index = 0;
    int saved_receiver_info_cnt = 0;

    // Loop to receive all receiver info
    while (1)
    {
        int read_bytes = read(sender_sock, &receive_struct, sizeof(receiver_t));
        if (read_bytes == -1)
        {
            error_handling("Receive_Info() error");
        }
        if (read_bytes == 0)
        {
            // End of file or no more data
            break;
        }

        // Check if the received port matches the current receiver's port
        if (ntohs(receive_struct.receiver_adr.sin_port) == this_port_num)
        {
            this_receiver = receive_struct;
        }
        else if (index < receiver_MAX)
        {
            receiver_array[index++] = receive_struct;
        }

        saved_receiver_info_cnt++;

        if (saved_receiver_info_cnt >= file_info->max_receiver_cnt)
        {
            break;
        }
    }

    return;
}

void Connect_To_Sender()
{

    // Connect to sender with information given from option
    sender_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&sender_adr, 0, sizeof(sender_adr));
    sender_adr.sin_family = AF_INET;
    sender_adr.sin_addr.s_addr = inet_addr(sender_address);
    sender_adr.sin_port = htons(sender_port_num);

    // First Connect to Server
    if (connect(sender_sock, (struct sockaddr *)&sender_adr, sizeof(sender_adr)) == -1)
    {
        error_handling("Connect_To_Sender() error");
    }

    // Send which Socket should be used for other Receivers to Connect too this Receiver
    receiver_t *receiver_struct = (receiver_t *)malloc(sizeof(receiver_t));
    receiver_struct->receiver_adr = this_adr; // saved as network byte order
    receiver_struct->nth_receiver = -1;

    if ((write(sender_sock, receiver_struct, sizeof(receiver_t))) == -1)
    {
        error_handling("Connect_To_Sender() error");
    }

    return;
}

#pragma endregion

#pragma region Helper Methods

int Initialize_Socket(int port_number)
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);

    int option = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    memset(&this_adr, 0, sizeof(this_adr));
    this_adr.sin_family = AF_INET;
    this_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    this_adr.sin_port = htons(port_number);

    if (bind(sock, (struct sockaddr *)&this_adr, sizeof(this_adr)) == -1)
        error_handling("bind() error");

    if (listen(sock, 5) == -1)
        error_handling("listen() error");

    return sock;
}

void GetOptArgs(int argc, char *argv[])
{
    int opt;

    // First check if the initial mode flag (-r / -s) is provided
    if ((opt = getopt(argc, argv, "rs")) == -1)
    {
        printf("Please start by entering -r (for receiver mode) / -s (for sender mode)\n");
        exit(1);
    }

    switch (opt)
    {
    // Receiver
    case 'r':
        rflag = 1;
        if (argc != 7)
        {
            printf("Receiver Usage: %s -r -a <sender ip:port> -p <this port>\n", argv[0]);
            exit(1);
        }

        while ((opt = getopt(argc, argv, "a:p:")) != -1)
        {
            switch (opt)
            {
            case 'a':
                // optind shows index of next element to process
                // So if port number is smaller than argc, which is 7,
                if (optind < argc)
                {
                    strcpy(sender_address, optarg);       // copy IP Addrerss
                    sender_port_num = atoi(argv[optind]); // copy port number on optind
                    optind++;                             // Skip next argument
                }
                else
                {
                    printf("Receiver Usage: %s -r -a <sender ip> <sender port> -p <this port>\n", argv[0]);
                    return;
                }
                break;
            case 'p':
                this_port_num = atoi(optarg);
                break;
            default:
                printf("Receiver Usage: %s -r -a <sender ip:port> -p <this port>\n", argv[0]);
                exit(1);
            }
        }
        break;

    // Sender
    case 's':
        sflag = 1;
        if (argc != 10)
        {
            printf("Sender Usage: %s -s -n <MAX Receiver count> -f <file_name> -g <segment_size> -p <this port>\n", argv[0]);
            exit(1);
        }
        while ((opt = getopt(argc, argv, "n:f:g:p:")) != -1)
        {
            switch (opt)
            {
            case 'n':
                receiver_MAX = atoi(optarg);
                receiver_array = (receiver_t *)malloc(sizeof(receiver_t) * receiver_MAX);
                recv_sock_array = (int *)malloc(sizeof(int) * receiver_MAX);
                break;
            case 'f':
                strcpy(file_name, optarg);
                break;
            case 'g':
                segment_size = atoi(optarg);
                segment_size *= 1000;
                break;
            case 'p':
                this_port_num = atoi(optarg);
                break;
            default:
                printf("Sender Usage: %s -s -n <MAX Receiver count> -f <file_name> -g <segment_size> -p <this port>\n", argv[0]);
                exit(1);
            }
        }
        break;
    default:
        printf("Please start by entering -r for receiver mode / -s for sender mode\n");
        exit(1);
    }
}


int Find_Nth_Receiver(int sockfd)
{
    int result = -1;
    for (int i = 0; i < receiver_MAX; i++)
    {
        if (recv_sock_array[i] == sockfd)
        {
            result = receiver_array[i].nth_receiver;
            break;
        }
    }
    return result;
}

void Close_Sockets()
{
    printf("Closing Sockets\n");
    for (int i = 0; i < receiver_MAX; i++)
    {
        close(recv_sock_array[i]);
    }
    close(this_sock);
}

void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

#pragma endregion

#pragma region Thread Methods

void *accpt_snd_seg(void *arg)
{
    // Wait (cond) for receivers to connect and accept with each other
    pthread_mutex_lock(&mutx);
    pthread_cond_wait(&cond, &mutx);
    pthread_mutex_unlock(&mutx);

    int buffer_size = file_info->segment_size;
    char buffer[buffer_size];

    // Allocate Memory for structure
    segment_t *file_segment;
    clock_t snd_start, snd_end;

    long read_cnt = 0;

    // Receive Segments from Sender
    for (int i = 0; i < this_receiver.segment_from_receiver_cnt; i++)
    {
        file_segment = (segment_t *)malloc(sizeof(segment_t));

        // Receive Segment Structure
        read_cnt = read(sender_sock, file_segment, sizeof(segment_t));
        if (read_cnt <= 0)
        {
            if (read_cnt == 0)
            {
                break;
            }
            error_handling("read() error for segment_t");
        }

        //printf("Received From Sender: %dnth Segment (%ldBytes)\n", file_segment->nthSegment, file_segment->sement_size);

        // Start Clock
        snd_start = clock();

        // Allocate memory for the segment data
        file_segment->segment = (char *)malloc(file_segment->sement_size);

        // Receive the actual data
        long file_len = 0;
        while (file_len < file_segment->sement_size)
        {
            read_cnt = read(sender_sock, file_segment->segment + file_len, file_segment->sement_size - file_len);
            if (read_cnt == -1)
            {
                error_handling("read() error while reading segment data");
            }
            else if (read_cnt == 0)
            {
                break; 
            }
            file_len += read_cnt;
        }

        // Process the received data
        segment_array[file_segment->nthSegment] = *file_segment;

        snd_end = clock();
        float elapsed_time = ((float)(snd_end - snd_start)) / CLOCKS_PER_SEC;

        pthread_mutex_lock(&mutx);
        accum_sent_len += file_segment->sement_size;
        Print_Receiver_Progress(current_segment_cnt, -1, file_segment->sement_size, elapsed_time);
        current_segment_cnt++;
        pthread_mutex_unlock(&mutx);

        // Make Thread to Send Segment to Receivers
        pthread_t flood_seg_thread;
        pthread_create(&flood_seg_thread, NULL, flood_segment, (void *)file_segment);
        pthread_join(flood_seg_thread, NULL);
    }

    return NULL;
}

void *flood_segment(void *arg)
{
    segment_t *file_segment = arg;
    // Flood all Receivers connected to this Receiver with Segment from Sender
    for (int i = 0; i < current_receiver_cnt; i++)
    {
        // Send the segment_t structure
        write(recv_sock_array[i], file_segment, sizeof(segment_t));

        // Send the actual data pointed to by segment
        write(recv_sock_array[i], file_segment->segment, file_segment->sement_size);
    }

    pthread_mutex_lock(&mutx);
    finished_segment_cnt++;
    pthread_mutex_unlock(&mutx);

    pthread_mutex_lock(&mutx);
    if (finished_segment_cnt >= file_info->segment_count)
    {
        pthread_cond_signal(&cond_finish);
    }
    pthread_mutex_unlock(&mutx);

    return NULL;
}


void *accpt_rcv_seg(void *arg)
{
    int recv_sock = *(int *)arg;
    free(arg); // Free the dynamically allocated memory for the socket descriptor
    clock_t recv_start, recv_end;

    segment_t *file_segment;

    long read_cnt = 0;

    // Receive Segments from Receiver
    while (1)
    {
        file_segment = (segment_t *)malloc(sizeof(segment_t));

        // Receive structure first
        read_cnt = read(recv_sock, file_segment, sizeof(segment_t));
        if (read_cnt <= 0)
        {
            if (read_cnt == 0)
            {
                // No more data
                break;
            }
            error_handling("read() error for segment_t");
        }

        recv_start = clock();

        // Allocate memory for the segment data
        file_segment->segment = (char *)malloc(file_segment->sement_size);

        // Receive the actual data
        long file_len = 0;
        while (file_len < file_segment->sement_size)
        {
            read_cnt = read(recv_sock, file_segment->segment + file_len, file_segment->sement_size - file_len);
            if (read_cnt == -1)
            {
                error_handling("read() error while reading segment data");
            }
            else if (read_cnt == 0)
            {
                break; // No more data
            }
            file_len += read_cnt;
        }

        //printf("Received From Receiver: %dnth Segment (%ldBytes)\n", file_segment->nthSegment, file_len);

        // Save to Segment Array
        segment_array[file_segment->nthSegment] = *file_segment;

        recv_end = clock();
        float elapsed_time = ((float)(recv_end - recv_start)) / CLOCKS_PER_SEC;

        pthread_mutex_lock(&mutx);
        accum_sent_len += file_segment->sement_size;
        Print_Receiver_Progress(current_segment_cnt, Find_Nth_Receiver(recv_sock), file_segment->sement_size, elapsed_time);
        current_segment_cnt++;
        finished_segment_cnt++;
        pthread_mutex_unlock(&mutx);

        if (current_segment_cnt >= file_info->segment_count)
            break;
    }

    pthread_mutex_lock(&mutx);
    if (finished_segment_cnt >= file_info->segment_count)
    {
        pthread_cond_signal(&cond_finish);
    }
    pthread_mutex_unlock(&mutx);
}

void *conn_receiver(void *arg)
{
    struct sockaddr_in addr;
    int sock;

    // For all Other Receivers Except its own,
    for (int i = this_receiver.nth_receiver; i < receiver_MAX; i++)
    {
        int index = i;

        sock = socket(PF_INET, SOCK_STREAM, 0);
        // Initiate Connection with those who are higher nth than its own
        if (connect(sock, (struct sockaddr *)&receiver_array[index].receiver_adr, sizeof(receiver_array[index].receiver_adr)) == -1)
        {
            error_handling("connect() error");
        }

        // Save Connected Sock in Array
        pthread_mutex_lock(&mutx);
        recv_sock_array[current_receiver_cnt++] = sock;
        pthread_mutex_unlock(&mutx);

        // Make thread to listen from this receiver
        pthread_t accpt_rcv_seg_thread;
        int *recv_sock_ptr = malloc(sizeof(int)); // Allocate memory for socket descriptor

        *recv_sock_ptr = sock;
        pthread_create(&accpt_rcv_seg_thread, NULL, accpt_rcv_seg, (void *)recv_sock_ptr);
        pthread_detach(accpt_rcv_seg_thread);
    }

    pthread_mutex_lock(&mutx);
    if (current_receiver_cnt >= receiver_MAX)
    {
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&mutx);

    return NULL;
}

void *accpt_receiver(void *arg)
{
    int recv_sock;
    struct sockaddr_in recv_adr;
    int recv_adr_sz;

    // Wait for connection from those lower than this nth
    for (int i = 0; i < this_receiver.nth_receiver; i++)
    {
        // Accept Another Receiver Connection
        recv_adr_sz = sizeof(recv_adr);
        recv_sock = accept(this_sock, (struct sockaddr *)&recv_adr, &recv_adr_sz);

        // Save its socket
        pthread_mutex_lock(&mutx);
        recv_sock_array[current_receiver_cnt++] = recv_sock;
        pthread_mutex_unlock(&mutx);

        pthread_t accpt_rcv_seg_thread;
        int *recv_sock_ptr = malloc(sizeof(int)); // Allocate memory for socket descriptor

        *recv_sock_ptr = recv_sock;
        pthread_create(&accpt_rcv_seg_thread, NULL, accpt_rcv_seg, (void *)recv_sock_ptr);
        pthread_detach(accpt_rcv_seg_thread);
    }

    pthread_mutex_lock(&mutx);
    if (current_receiver_cnt >= receiver_MAX)
    {
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&mutx);

    return NULL;
}

#pragma endregion