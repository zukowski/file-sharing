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
#define USER_INPUT_LEN 256
#define TIMEOUT 1000

char client_name[USERNAME_LEN];
struct UserFile *my_files = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int port;

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

  get_response(sockfd);
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
    struct UserFile *file = malloc(sizeof(struct UserFile));
    memcpy(file,buf,sizeof(struct UserFile));
    printf("File info: %s %s\n",file->filename,file->owner_ip);
    receive_file(file->filename, file->owner_ip); 
  } 
  if (n < 0) error("ERROR reading from socket");
}

void get_file(int sockfd, char *filename) {
  int n;
  char *buf = calloc(strlen(GET_CMD) + 1 + strlen(filename),1);
  strcpy(buf,GET_CMD);
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
  /*
  selectTimeout.tv_sec = TIMEOUT / 1000;
  selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000;

  FD_ZERO(&rdset);
  FD_ZERO(&wrset);
  
  // start monitoring for reads or timeout
  if (s <= 0) FD_SET(sockfd, &rdset);
  s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 
  if (s == -1) { printf("ERROR: Socket error. Exiting.\n"); exit(1); }
  if (s > 0) {
    n = read(sockfd,msgbuf,99);
  }
  if (s == 0) {
    printf("No response from server. Please try again.\n");
  }
  */
}

void *handle_xfer_requests(void *my_port) { 
  pthread_detach(pthread_self()); //automatically clears the threads memory on exit
  int sockfd; 
  struct sockaddr_in serv_addr;
     
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  printf("Xfer port #: %d\n",port+1);
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
      printf("Awaiting peer connections...\n");
      sleep(1);
    }

    if (s > 0) {
      printf("Peer request received\n");
      int n, r;
      socklen_t clilen;
      int newsockfd;
      struct sockaddr_in cli_addr;
      char buffer[100];
      char welcome_msg[100];
      pthread_t th;

      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
      printf("New sockid: %d\n",newsockfd);

      // send welcome message 
      strcpy(welcome_msg,"Connected with peer.\n");
      n = write(newsockfd,welcome_msg,strlen(welcome_msg)); 
    }
  }

  close(sockfd);
  
  return 0;
}

void *handle_user_input(void *sockfd) {
  pthread_detach(pthread_self()); //automatically clears the threads memory on exit
  char *user_input = malloc(USER_INPUT_LEN);
  char *command = malloc(COMMAND_LEN);
  char *command_arg = malloc(FILENAME_LEN);
  int _sockfd = *(int *)sockfd;
  
  for(;;) {
    bzero(user_input,USER_INPUT_LEN);
    bzero(command,COMMAND_LEN);
    bzero(user_input,FILENAME_LEN);

    gets(user_input);
    command = strtok(user_input," ");  
    command_arg = strtok(NULL,"\0");

    if (strcmp(command,LIST_CMD) == 0) {
      pthread_mutex_lock(&mutex);
      list_files_available(_sockfd);
      pthread_mutex_unlock(&mutex);
    }
    else if (strcmp(command,GET_CMD) == 0) {
      get_file(_sockfd, command_arg);
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
      printf("SendMyfilesList\tSend list of shared files to server.\n");
      printf("Get <filename>\tGet a file from the network.\n");
    }
    else if (strcmp(command,EXIT_CMD) == 0) {
      // TODO
      // teardown connection
      close(_sockfd);
      break;
    }
    else { 
      printf("Not a valid command. Type 'usage' for help.\n");
    }
  }
  return 0;
}

// listens for incoming requests for file transfer
void *connection(void *sockfd) {
  /*int _sockfd = (int) sockfd;
  fd_set rdset, wrset;
  int s = -1;
  int max_fd = _sockfd + 1;
  struct timeval selectTimeout;
  
  selectTimeout.tv_sec = TIMEOUT / 1000;
  selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000;

  FD_ZERO(&rdset);
  FD_ZERO(&wrset);
  */
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
