/*
 * Example of client using TCP protocol.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h>
#include <pthread.h>
#include "my_utils.h"

#define SHARED_DIR "./Shared files"
#define DL_DIR "./Downloaded files"
#define USER_INPUT_LEN 256
#define MAX_FILE_CHUNK 10490000 // 10MB
#define STOP_FLAG "[!!!STOP!!!]"
#define TIMEOUT 1000
#define MAX_TIMEOUTS 5

char client_name[USERNAME_LEN];
struct UserFile *my_files = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int port;
int sending = 0;

// You may/may not use pthread for the client code. The client is communicating with
// the server most of the time until he recieves a "GET <file>" request from another client.
// You can be creative here and design a code similar to the server to handle multiple connections.
void build_filelist(int *bufsize, char *filelist_buf) {
  struct UserFile *f;
  for(f = my_files; f != NULL; f = f->hh.next) {
    memcpy(filelist_buf,f,sizeof(struct UserFile));
    *bufsize += sizeof(struct UserFile);
    filelist_buf += sizeof(struct UserFile);
  }
}

void get_response(int sockfd) {
  int n;
  fd_set rdset, wrset;
  int s = -1;
  int max_fd = sockfd + 1;
  struct timeval selectTimeout;
  char *msg_buf = calloc(sizeof(struct UserFile)*FILELIST_LEN,1);
  
  selectTimeout.tv_sec = TIMEOUT / 1000;
  selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000;

  FD_ZERO(&rdset);
  FD_ZERO(&wrset);
  
  // start monitoring for reads or timeout
  if (s <= 0) FD_SET(sockfd, &rdset);
  s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 
  if (s == -1) { printf("ERROR: Socket error. Exiting.\n"); exit(1); }

  if (s > 0) {
    n = read(sockfd,msg_buf,1024);
    printf("%s\n",msg_buf);
  }
 
  if (n < 0) error("ERROR reading from socket");
}

void receive_file_data(int sockfd, char *filename) {
  int n = 1;
  FILE *fp;
  char *filepath = calloc(128,1);
  char *buf;
  strcpy(filepath,DL_DIR);
  strcat(filepath,"/");
  strcat(filepath,filename);
  fp = fopen(filepath,"w");
  fclose(fp); 
  fd_set rdset, wrset;
  int s = -1;
  int max_fd = sockfd + 1;
  struct timeval selectTimeout;
  
  selectTimeout.tv_sec = TIMEOUT / 1000;
  selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000;
  
  int to = 0;
  
  while (n > 0) {
    FD_ZERO(&rdset);
    FD_ZERO(&wrset);
    selectTimeout.tv_sec = TIMEOUT / 1000;
    selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000;
 
    // start monitoring for reads or timeout
    if (s <= 0) FD_SET(sockfd, &rdset);
    
    s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 
    
    if (s == -1) { printf("ERROR: Socket error. Exiting.\n"); exit(1); }
    if (s == 0) {
      to++;
      if (to >= MAX_TIMEOUTS) {
        break;
      }
    }
   
    if (s > 0) {
      to = 0;
      buf = calloc(MAX_FILE_CHUNK,1);
      n = recv(sockfd,buf,MAX_FILE_CHUNK,0);
      if (n < 0) printf("Some error reading socket\n"); 
      if (n > 0) {
        if (strcmp(buf,STOP_FLAG) == 0) {
          //printf("Stop flag found\n");
          break;
        }
        fp = fopen(filepath,"a");
        fwrite(buf,1,n,fp);
        fclose(fp);
      }
      free(buf);    
    }
  }
  printf("Finished downloading file\n");
}

void receive_file(char *filename, char *ip) {
  int sockfd, i, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");
  
  server = gethostbyname(ip);
  
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
  }
    
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
  serv_addr.sin_port = htons(port+1);
 
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    error("ERROR connecting");

  char *file_request = malloc(128);
  sprintf(file_request,"%s %s",filename,client_name);
  
  // send the filename and client_name to the peer
  n = send(sockfd,file_request,strlen(file_request),0);

  // get data
  receive_file_data(sockfd, filename);
  
  // close connection
  close(sockfd);
}

void receive_file_info(int sockfd) {
  int n;
  fd_set rdset, wrset;
  int s = -1;
  int max_fd = sockfd + 1;
  struct timeval selectTimeout;
  char *buf = calloc(sizeof(struct UserFile),1);
  
  selectTimeout.tv_sec = TIMEOUT / 1000;
  selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000;

  FD_ZERO(&rdset);
  FD_ZERO(&wrset);
  
  // start monitoring for reads or timeout
  if (s <= 0) FD_SET(sockfd, &rdset);
  s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 
  if (s == -1) { printf("ERROR: Socket error. Exiting.\n"); exit(1); }

  if (s > 0) {
    n = read(sockfd,buf,sizeof(struct UserFile));
    if (strcmp(buf,MSG_BUSY) == 0) {
      printf(MSG_BUSY);
      printf("\n");
    }
    else if (strcmp(buf,MSG_FILE_NA) == 0) {
      printf("%s\n",MSG_FILE_NA);
    }
    else {
      struct UserFile *file = malloc(sizeof(struct UserFile));
      memcpy(file,buf,sizeof(struct UserFile));
      printf("Start downloading %s from %s\n",file->filename,file->owner);
      receive_file(file->filename, file->owner_ip);
    }
  } 
  if (n < 0) error("ERROR reading from socket");
}

void get_file(int sockfd, char *filename) {
  int n;
  char *buf = calloc(128,1);
  strcat(buf,GET_CMD);
  strcat(buf," ");
  strcat(buf,filename);
  n = send(sockfd,buf,strlen(buf),0);
  if (n < 0) error("ERROR writing to socket");
  receive_file_info(sockfd);
}

void list_files_available(int sockfd) {
  int n;
  n = write(sockfd,LIST_CMD,sizeof(LIST_CMD));
  if (n < 0) error("ERROR writing to socket");
  get_response(sockfd);
}

void user_list(int sockfd) {
  int n;
  n = send(sockfd,USERS_CMD,sizeof(USERS_CMD),0);
  if (n < 0) error("ERROR writing to socket");
  get_response(sockfd); 
}

void send_client_name(int sockfd, char *client_name) {
  int n;
  n = send(sockfd,client_name,strlen(client_name),0);
  if (n < 0) error("ERROR writing to socket");
  get_response(sockfd); 
}

void send_file_list(int sockfd) {
  int n;
  char *filelist_buf = malloc(sizeof(struct UserFile)*FILELIST_LEN);
  bzero(filelist_buf,sizeof(filelist_buf));
  int bufsize = 0;
  char msgbuf[100];
  build_filelist(&bufsize, filelist_buf);
  n = write(sockfd,SEND_LIST_CMD,sizeof(SEND_LIST_CMD));
  n = write(sockfd,filelist_buf,bufsize);
  if (n < 0) error("Error writing to socket");   
  get_response(sockfd);
}

void *handle_xfer_requests(void *my_port) { 
  pthread_detach(pthread_self()); //automatically clears the threads memory on exit
  int sockfd; 
  int port = *(int *)my_port;
  struct sockaddr_in serv_addr;
     
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port+1);
  
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    error("ERROR on binding");
  }

  listen(sockfd,5);
  
  fd_set rdset, wrset;
  int s = -1;
  int max_fd = sockfd + 1;
  struct timeval selectTimeout;
  
  selectTimeout.tv_sec = 1000 / 1000;
  selectTimeout.tv_usec = (1000 % 1000) * 1000;
 
  for(;;) { 
    s = -1;
    FD_ZERO(&rdset);
    FD_ZERO(&wrset);

    // start monitoring for reads or timeout
    if (s <= 0) FD_SET(sockfd, &rdset);
  
    s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 
    if (s == -1) { printf("ERROR: Socket error. Exiting.\n"); exit(1); }

    if (s == 0) {
      //printf("Awaiting peer connections...\n");
      sleep(1);
    }

    if (s > 0) {
      int n;
      int r;
      socklen_t clilen;
      int newsockfd;
      struct sockaddr_in cli_addr;
      char buffer[100];
      bzero(buffer,100);
      char owner[USERNAME_LEN];
      char welcome_msg[100];
      pthread_t th;
      
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
      
      if (sending == 1) {
        n = send(newsockfd,MSG_BUSY,strlen(MSG_BUSY),0);  
        close(newsockfd);
        continue;
      }
      else {
        sending = 1;
        
        // extract filename
        char *file_request = malloc(FILENAME_LEN + 1 + USERNAME_LEN);
        n = recv(newsockfd,file_request,FILENAME_LEN + 1 +USERNAME_LEN,0);

        char *filename = malloc(FILENAME_LEN);
        char *owner = malloc(USERNAME_LEN);
        filename = strtok(file_request," ");
        owner = strtok(NULL,"\0");
 
        // open file, buffer, and send data
        FILE *fp;
        char *filepath = malloc(128);
        char *buf = calloc(MAX_FILE_CHUNK,1);
        int m;
        strcpy(filepath,SHARED_DIR);
        strcat(filepath,"/");
        strcat(filepath,filename);
        fp = fopen(filepath,"r");
        m = 1;
        n = 1;
        printf("Sending file %s to client %s\n",filename, owner);
        while (n > 0 && feof(fp) == 0) {
          m = fread(buf,1,MAX_FILE_CHUNK,fp);
          n = send(newsockfd,buf,m,0);
        }
        close(newsockfd);
        sending = 0;
      }
    }
  }

  close(sockfd);
   
  return 0;
}

void *handle_user_input(void *sockfd) {
  pthread_detach(pthread_self()); //automatically clears the threads memory on exit
  char *user_input = malloc(USER_INPUT_LEN);
  char *command = malloc(COMMAND_LEN);
  char *filename = malloc(FILENAME_LEN);
  int _sockfd = *(int *)sockfd;
  
  for(;;) {
    bzero(user_input,USER_INPUT_LEN);
    bzero(command,COMMAND_LEN);
    bzero(user_input,FILENAME_LEN);

    gets(user_input);
    command = strtok(user_input," ");  
    if (strcmp(command,LIST_CMD) == 0) {
      list_files_available(_sockfd);
    }
    else if (strcmp(command,GET_CMD) == 0) { 
      filename = strtok(NULL," ");
      get_file(_sockfd, filename);
    }
    else if (strcmp(command,USERS_CMD) == 0) {
      user_list(_sockfd);
    }
    else if (strcmp(command,SEND_LIST_CMD) == 0) {
      send_file_list(_sockfd);
    }
    else if (strcmp(command,USAGE_CMD) ==0) {
      printf("List\t\t\tList available files.\n");
      printf("Users\t\t\tList active users.\n");
      printf("SendMyfilesList\t\tSend list of shared files to server.\n");
      printf("Get <filename>\t\tGet a file from the network.\n");
    }
    else if (strcmp(command,EXIT_CMD) == 0) {
      // TODO
      // teardown connection
      close(_sockfd);
      break;
    }
    else { 
      printf("Not a valid command. Type 'Usage' for help.\n");
    }
  }
  return 0;
}

int main(int argc, char *argv[])
{
  int sockfd, srv_port, i, n, r1, r2;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  pthread_t th1, th2;
  char *msg_buf = malloc(1024);
  
  if (argc < 5) {
    fprintf(stderr,"usage: %s <client name> <server ip> <server port #> <list of files from “shared files directory”>\n", argv[0]);
    exit(0);
  }

  strcpy(client_name,argv[1]);

  // store list of files in hash
  for(i=4;i<argc;i++) {
    FILE *fp;
    struct UserFile *file;
    char filepath[100];
    strcpy(filepath,SHARED_DIR);
    strcat(filepath,"/");
    strcat(filepath,argv[i]);
    file = malloc(sizeof(struct UserFile));
    fp = fopen(filepath,"r");
    fseek(fp, 0, SEEK_END); // seek to end of file
    int size = ftell(fp); // get current file pointer
    fseek(fp, 0, SEEK_SET); // seek back to beginning of file
    fclose(fp);
    strcpy(file->filename,argv[i]);
    sprintf(file->filesize,"%d",size);
    strcpy(file->owner,client_name);
    strcpy(file->owner_ip,"127.0.0.1");
    HASH_ADD_INT(my_files, filename, file);
  }

  port = atoi(argv[3]);
   
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");
  
  server = gethostbyname(argv[2]);
  
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host!!!\n");
  }
    
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
  serv_addr.sin_port = htons(port);
  
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    error("ERROR connecting");
  
  send_client_name(sockfd,client_name);

  // handle user input
  r1 = pthread_create(&th1, 0, handle_user_input, (void *)&sockfd);
  if (r1 != 0) { fprintf(stderr, "thread create failed\n"); }

  // handle file transfer requests
  r2 = pthread_create(&th2, 0, handle_xfer_requests, (void *)&port);
  if (r2 != 0) { fprintf(stderr, "thread create failed\n"); } 

  for(;;) {
    sleep(1);
  }

    
    /**********************************************
    // TODO: move the listening for messages to a seaprate thread

    
    if (s == 0) { // timeout, nothing to recv
      if (waiting_for_input == 0) {
        printf("> ");
      }
      gets(user_input);
      command = strtok(user_input," ");      
    }
    **********************************************/
  
  return 0;
}
