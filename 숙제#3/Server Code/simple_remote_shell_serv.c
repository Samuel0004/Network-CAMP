#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <libgen.h>
#include <time.h>

#define FILE_COUNT 64   // Max File Count Per Client in Serv Array
#define BUFSIZE 4096
#define FILE_NAME 64
#define EPOLL_SIZE 50

enum Option {
  DOWNLOAD,
  UPLOAD,
  MOVEDIR,
  SHOW
};

// Typedefine structure of file (contain name and file size)
typedef struct file_t{
    char file_name[FILE_NAME];
    long file_size;
    int is_directory; // 1 for directory 
}file_t; //alias as file_t

typedef struct client_t{
    int sockfd;
    char directory[FILE_NAME];
    int entry_count;
    file_t file_t_array[FILE_COUNT];
}client_t;

typedef struct request_t{
    int option;
    char request_text[FILE_NAME];
}request_t;

typedef struct argument_t{
    int client_sock;
    request_t *request;
}argument_t;

void Print_Dir_Info(client_t *client_struct);
void CreateDefaultDirectory(int serv_sock);
client_t* CreateNewFromDefault(int new_sockfd);
void error_handling(char *message);
int Initialize_Socket(struct sockaddr_in serv_adr, int port_number);
void Send_Directory(client_t *client_struct);
void CreateArrayFromDirectory(client_t *client_struct);
client_t* GetStructWithSock(int sockfd);
void* HandleClientRequest(void* arg);
void* HandleClientConnection(void* arg);
file_t* SearchFile(argument_t *argument);
void SendFile(argument_t *argument);
void GetFile(argument_t *argument);
void MoveDir(argument_t *argument);
void ShowDir(argument_t *argument);
void UpdateDirectory(client_t *client_struct, char* new_dir);

int client_cnt = 0;
client_t* client_t_array[EPOLL_SIZE];
client_t* default_directory;

pthread_mutex_t mutex;

int main(int argc, char* argv[]){
    int str_len, i;
    int serv_sock;
	struct sockaddr_in serv_adr, clnt_adr;
    
    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;
    
    socklen_t adr_sz;
    pthread_t snd_thread, rcv_thread;

    char buf[BUFSIZE];
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutex,NULL);

    // Initialize Socket
    serv_sock = Initialize_Socket(serv_adr, atoi(argv[1]));

    int option = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    
    // Initialize EPOLL
    epfd=epoll_create(EPOLL_SIZE);
    ep_events=malloc(sizeof(struct epoll_event)*EPOLL_SIZE);
    event.events=EPOLLIN;
    event.data.fd=serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

    // Default Directory to use for all new Clinet Request
    CreateDefaultDirectory(serv_sock);

    while(1)
    {
        event_cnt=epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);

        if(event_cnt==-1){
            puts("epoll_wait() error");
            break;
        }
        
        for(i=0; i<event_cnt; i++){

            if(ep_events[i].data.fd==serv_sock){
                // New Client Connection
                int clnt_sock;
                
                adr_sz=sizeof(clnt_adr);
                clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);

                //pthread_mutex_lock(&mutex);
                event.events=EPOLLIN;
                event.data.fd=clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                //pthread_mutex_unlock(&mutex);

                // Handle New Conenction Request by Client
                pthread_create(&rcv_thread, NULL, HandleClientConnection, (void*)&clnt_sock);
                pthread_join(rcv_thread,NULL);

            }       
            else{
                // Client SSH Request
                
                request_t *client_request = (request_t *)malloc(sizeof(request_t));
                str_len=read(ep_events[i].data.fd, client_request, sizeof(request_t));
                if(str_len<=0) {
                    // close request!
                    
                    pthread_mutex_lock(&mutex);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
                    client_cnt--;
                    pthread_mutex_unlock(&mutex);
                    
                    close(ep_events[i].data.fd);
                    
                    printf("closed client: %d \n", ep_events[i].data.fd);

                }
                else{
                    // Some Request From Client
                    argument_t *argument = (argument_t*)malloc(sizeof(argument_t)); 
                    
                    argument->client_sock = ep_events[i].data.fd;
                    argument->request = client_request;

                    pthread_create(&snd_thread, NULL, HandleClientRequest, (void*)argument);
                    pthread_join(snd_thread,NULL);

                }
            }
        }
    
    }
        
    close(serv_sock);
    close(epfd);
    return 0;
}

void UpdateDirectory(client_t *client_struct, char* new_dir){
    // Update Directory
    strcpy(client_struct->directory, new_dir);
    
    // Update Array
    CreateArrayFromDirectory(client_struct);
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

void CreateDefaultDirectory(int serv_sock){
    default_directory = (client_t*)malloc(sizeof(client_t));

    default_directory->sockfd = serv_sock;
    //strcpy(default_directory->directory,"./");

    char actualpath[FILE_NAME];
    char *ptr = realpath("./",actualpath);

    strcat(ptr,"/");
    strcpy(default_directory->directory, ptr);

    CreateArrayFromDirectory(default_directory);
}

client_t* CreateNewFromDefault(int new_sockfd){

    client_t *client_struct = (client_t*)malloc(sizeof(client_t));
    client_struct->sockfd = new_sockfd;
    client_struct->entry_count = default_directory->entry_count;
    strcpy(client_struct->directory, default_directory->directory);
    memcpy(client_struct->file_t_array, default_directory->file_t_array, sizeof(default_directory->file_t_array));

    return client_struct;
}

void CreateArrayFromDirectory(client_t *client_struct){
    DIR* dir;
    struct dirent* entry;
    FILE* open_file;
    long file_size;
    char dirname[FILE_NAME];

    strcpy(dirname, client_struct->directory);
    dir = opendir(dirname);

    if(!dir)
        error_handling("opendir() error");

    int count = 0;

    //Clear Array 
    memset(client_struct->file_t_array, 0, sizeof(client_struct->file_t_array));

    // Populate with Most Recent Info
    while((entry = readdir(dir))!= NULL){
        if(strcmp(entry->d_name,".") ==0)
            continue;
        
        // Get Full name
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", dirname, entry->d_name);
        
        if(entry->d_type == DT_REG){
            // Case Registery

            open_file = fopen(entry->d_name,"rb");
            if (!open_file) {
                //printf("NO ACCESS: %s\n",entry->d_name);
                continue;  // Skip files with no access to read, this way client has same accessibility as server
            }

            fseek(open_file,0,SEEK_END);
            file_size = ftell(open_file);
            
            if(count<FILE_COUNT){
                strcpy(client_struct->file_t_array[count].file_name,entry->d_name);
                client_struct->file_t_array[count].file_size = file_size;
                client_struct->file_t_array[count].is_directory = 0;  

                //printf("%s %ld\n", client_struct->file_t_array[count].file_name,client_struct->file_t_array[count].file_size );
                count++;
            }

            fclose(open_file);
        }
        else if (entry->d_type == DT_DIR){
            // Case Directory

            if (count < FILE_COUNT) {
                strcpy(client_struct->file_t_array[count].file_name, entry->d_name);
                client_struct->file_t_array[count].file_size = 0;  // Size for directory 0
                client_struct->file_t_array[count].is_directory = 1;  
                count++;
            }
        }
    }

    client_struct->entry_count = count;
    closedir(dir);
    return;
}


void* HandleClientRequest(void* arg){
    argument_t *argument = arg;

    switch(argument->request->option){
        case DOWNLOAD:
            SendFile(argument);
            break;  
        case UPLOAD:
            GetFile(argument);
            break;
        case MOVEDIR:
            MoveDir(argument);
            break;
        case SHOW:
            ShowDir(argument);
            break;
        default:
            printf("Wrong command from client.\n");
            break;
    }

    return NULL;
}

void MoveDir(argument_t *argument){
    client_t *client_struct = GetStructWithSock(argument->client_sock);
    char new_dir[512];
    // Means Move up to Parent Directory
    if(strcmp(argument->request->request_text, "..")==0){

        // Cannot move higher than where the server is at 
        if(strcmp(client_struct->directory, "./")==0){
            printf("Cannot move higher than where the server is at \n");
            return;
        }
            
        strcpy(new_dir, client_struct->directory);
        int len = sizeof(new_dir);
        
        // Remove '/'
        if(len>0 && new_dir[len-1] == '/'){
            new_dir[len-1]='\0';
        }
        
        // Get Parent Directory with dirname() function fron libgen library
        char *parent_directory = dirname(new_dir);

        if (parent_directory != NULL && strlen(parent_directory) > 0) {
            strcpy(new_dir, parent_directory);
            strcat(new_dir,"/");
            printf("New Directory after moving up: %s\n", new_dir);
            UpdateDirectory(client_struct, new_dir);
        } else {
            printf("Already at the root directory\n");
        }

    }
    else{
        snprintf(new_dir, sizeof(new_dir), "%s%s/", client_struct->directory,argument->request->request_text);
        printf("New Directory: %s\n",new_dir);
        UpdateDirectory(client_struct, new_dir);
    }
}

void ShowDir(argument_t *argument){
    client_t *client_struct = GetStructWithSock(argument->client_sock);
    // Update Directory
    UpdateDirectory(client_struct,client_struct->directory);
    Send_Directory(client_struct);
}

void GetFile(argument_t *argument){
    file_t *receive_file = (file_t*)malloc(sizeof(file_t));

    // First Get File info from Client
    if(read(argument->client_sock, receive_file, sizeof(file_t)) == -1){
        error_handling("read() error");
    }
    
    FILE *fp;
    char buf[BUFSIZE];
    int read_cnt = 0;
    long file_len = 0;

    client_t *client_struct = (client_t*)malloc(sizeof(client_t));
    client_struct = GetStructWithSock(argument->client_sock);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", client_struct->directory, receive_file->file_name);
    
    // Open a file with chosen file name to write
    fp = fopen(filepath, "wb");
    while ((read_cnt = read(argument->client_sock, buf, BUFSIZE)) != 0)
    {
        if (read_cnt == -1)
            error_handling("read() error");

        file_len += read_cnt;

        fwrite((void *)buf, 1, read_cnt, fp);
        
        if (file_len >= receive_file->file_size)
            break;
    }

    printf("Uploaded File to Serv: %s\n",receive_file->file_name);
    fclose(fp);

    return;
}


void SendFile(argument_t *argument){
    char buffer[BUFSIZE];
    FILE* fp;
    file_t *file = SearchFile(argument);
    
    if(file == NULL){
        printf("No such file in current Directory\n");
        return;
    }
    
    fp = fopen(file->file_name, "rb");
    
    while (1) {
        size_t bytes_read = fread(buffer, 1, BUFSIZE, fp);
        if (bytes_read > 0) {
            send( argument->client_sock, buffer, bytes_read, 0);
        }
        if (bytes_read < BUFSIZE) {
            if (ferror(fp)) {
                error_handling("fread() error");
            }
            break;
        }
    }

    fclose(fp);
    return;
}

file_t* SearchFile(argument_t *argument){
    int i;
    int client_index = -1;
    int file_index = -1;
    file_t *file = NULL;
    client_t *client_struct = NULL;
    
    pthread_mutex_lock(&mutex);

    for(int i=0;i<client_cnt;i++){
        if(client_t_array[i]->sockfd == argument->client_sock){
            client_struct = client_t_array[i];            
            break;
        }
    }

    for(int i=0;i<client_struct->entry_count;i++){
        if(!strcmp(client_struct->file_t_array[i].file_name, argument->request->request_text)){
            file = &client_struct->file_t_array[i];
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    return file;
}

void* HandleClientConnection(void* arg){
    int clnt_sock = *((int*)arg);

    printf("connected client: %d \n", clnt_sock);

    // Create new client_t with starting directory and client socket fd 
    client_t *client_struct = CreateNewFromDefault(clnt_sock);

    // Save it in an Array
    pthread_mutex_lock(&mutex);
    client_t_array[client_cnt++] = client_struct;            
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void Send_Directory(client_t *client_struct){
    pthread_mutex_lock(&mutex);
    // First send the Number of file_t
    write(client_struct->sockfd, &client_struct->entry_count, sizeof(client_struct->entry_count));

    // Then Send file_t struct one by one
    for(int i=0;i<client_struct->entry_count;i++){
        //printf("%s %ld\n", client_struct->file_t_array[i].file_name, client_struct->file_t_array[i].file_size);
        write(client_struct->sockfd, &client_struct->file_t_array[i], sizeof(file_t));
    }
    pthread_mutex_unlock(&mutex);
    return;
}

void Print_Dir_Info(client_t *client_struct){
    printf("%d\n",client_struct->sockfd); 
    printf("%s\n",client_struct->directory); 
     for(int i=0;i<client_struct->entry_count;i++){
        printf("%-12s %-12ld\n", client_struct->file_t_array[i].file_name,client_struct->file_t_array[i].file_size);
    }
    return;
}


client_t* GetStructWithSock(int client_sock){
    client_t* result = NULL;
    pthread_mutex_lock(&mutex);
    for(int i=0;i<client_cnt;i++){
        if(client_t_array[i]->sockfd == client_sock){
            result =  client_t_array[i];            
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    return result;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
