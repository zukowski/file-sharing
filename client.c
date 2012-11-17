/*
  Author: Daniel Zukowski <daniel.zukowski@gmail.com>
  Date: Nov 16, 2012
 
  A basic TCP file transfer client.
  The client connects to the main server to share its list of
  available files and to see a list of files that other peers are sharing.
  When a file is requested, the server sends the IP of the user who owns the file.
  The client then initiates a unique connection to the client with connect().
  If the peer accept()'s the connection, the peer then begins streaming the file
  segments backt to the client. Upon completion of the file transfer, both sides
  teardown and close() the connection.

  The port number used for P2P file transfers is one greater than the port number used to
  communicate with the server.

  Constants and Macros defined in app_utils.h
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
#include <time.h>
#include "my_utils.h"


//------------------------------------------------
// Global vars                                    
//------------------------------------------------

// from argv: stores the client's identifying name
char client_name[USERNAME_LEN];

// from argv: port # of the TCP server
int port;

// mutex for atomic actions
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// is the server
int sending = 0;

// hash table of client files
struct UserFile *my_files = NULL;

// expecting file info flag
int expecting_file_info = 0;

//------------------------------------------------
// Functions                                    
//------------------------------------------------

/**
 * Removes the trailing newline character from stdin
 * input read in by fgets
 */
char *remove_newline(char *s)
{
  int len = strlen(s);
  if (len > 0 && s[len-1] == '\n')  // if there's a newline
    s[len-1] = '\0';                // truncate the string
  return s;
}

/**
 * Builds a formatted string depicting a table of file names
 */
void build_filelist(int *bufsize, char *filelist_buf)
{
  struct UserFile *f;
  for(f = my_files; f != NULL; f = f->hh.next) {
    memcpy(filelist_buf,f,sizeof(struct UserFile));
    *bufsize += sizeof(struct UserFile);
    filelist_buf += sizeof(struct UserFile);
  }
}

/**
 * Receives a response from the server and display it
 */
void get_response(int sockfd)
{
  int n = 1;
  char *msg_buf = calloc(sizeof(struct UserFile)*FILELIST_LEN,1);
  SELECT_INIT();
  
  for (;;) {
    SELECT();
    if (s > 0) {
      n = recv(sockfd,msg_buf,1024,0);
      if (n > 0) {
        printf("%s\n",msg_buf);
      }
      else if (n == 0) {
        printf("%s\n",MSG_EMPTY);
      }
      else {
        error(RECV_ERR);
      }
      break;
    }
    if (s == 0) {
      // Timeout
    }
  }
}

/**
 * Receives the data stream for a file transfer and
 * writes the contents to a local file.
 */
void receive_file_data(int sockfd, char *filename)
{
  int n = 1;   
  int to = 0; // timeout counter
  FILE *fp;
  char *filepath = calloc(128,1);
  char *buf;
 
  // format the filepath to include the directory name 
  sprintf(filepath,"%s/%s",DL_DIR,filename);
  
  // quick open/close with "w" to create/overwrite existing file
  fp = fopen(filepath,"w");
  fclose(fp); 
 
  // set up the inputs for select() monitoring
  SELECT_INIT();
    
  // keep looping while there's data to receive 
  do {
    SELECT();   
    // if timeout occurred, increase counter.
    // stop reading socket if too many consecutive timeouts
    if (s == 0) {
      to++;
      if (to >= MAX_TIMEOUTS) {
        break;
      }
    }
   
    // there is data to read, so now read it into a file
    if (s > 0) {
      to = 0;
      buf = calloc(MAX_FILE_CHUNK,1);
      n = recv(sockfd,buf,MAX_FILE_CHUNK,0);
      if (n < 0) printf("Some error reading socket\n"); 
      if (n > 0) {
        fp = fopen(filepath,"a");
        fwrite(buf,1,n,fp);
        fclose(fp);
      }
      free(buf);    
    }
  } while (n > 0);

  printf("Finished downloading file\n");
}

/**
 * Starts the process of receiving a file transfer from a peer.
 */
void receive_file(char *filename, char *ip)
{
  int sockfd, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  
  // set up TCP socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");
 
  // verify the IP address belongs to a host 
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

  // request connection to peer (TCP handshaking) 
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    error("ERROR connecting");

  // send the filename and our client_name to the peer
  char *file_request = malloc(128);
  sprintf(file_request,"%s %s",filename,client_name); 
  n = send(sockfd,file_request,strlen(file_request),0);

  // now start to try to receive the data from the stream
  receive_file_data(sockfd, filename);
  
  // at this point, the file transfer has either
  // completed or failed, so close the connection
  close(sockfd);
}

/**
 * Receives a byte stream representing a UserFile struct
 */
void receive_file_info(int sockfd)
{
  int n = 1;
  char *buf = calloc(sizeof(struct UserFile),1);
  SELECT_INIT(); 
  
  // start monitoring for reads or timeout
  for (;;) {
    SELECT(); 
    if (s > 0) {
      n = recv(sockfd,buf,sizeof(struct UserFile),0);
      if (n < 0) error(RECV_ERR);
      if (strcmp(buf,MSG_BUSY) == 0) {
        printf(MSG_BUSY);
        printf("\n");
        break;
      }
      else if (strcmp(buf,MSG_FILE_NA) == 0) {
        printf("%s\n",MSG_FILE_NA);
        break;
      }
      else {
        struct UserFile *file = malloc(sizeof(struct UserFile));
        memcpy(file,buf,sizeof(struct UserFile));
        printf("Start downloading %s from %s\n",file->filename,file->owner);
        receive_file(file->filename, file->owner_ip);
        break;
      }
    }
  } 
}

/**
 * Sends the GET_CMD and argument to the server,
 * initiating a request for a file download.
 */
void get_file(int sockfd, char *filename)
{
  int n;
  char *buf = calloc(128,1);
  sprintf(buf,"%s %s",GET_CMD,filename);
  n = send(sockfd,buf,strlen(buf),0);
  if (n < 0) error("ERROR writing to socket");
  receive_file_info(sockfd);
}

/**
 * Sends LIST_CMD command to the server,
 * asking for a list of currently available files.
 */
void list_files_available(int sockfd)
{
  int n;
  n = write(sockfd,LIST_CMD,sizeof(LIST_CMD));
  if (n < 0) error("ERROR writing to socket");
  get_response(sockfd);
}

/**
 * Sends the USERS_CMD to the server,
 * asking for a list of currently connected users.
 */
void user_list(int sockfd)
{
  int n;
  n = send(sockfd,USERS_CMD,sizeof(USERS_CMD),0);
  if (n < 0) error("ERROR writing to socket");
  get_response(sockfd); 
}

/**
 * Sends the client_name to the server,
 * this is used when first connecting, to establish identity.
 */
void send_client_name(int sockfd, char *client_name)
{
  int n;
  n = send(sockfd,client_name,strlen(client_name),0);
  if (n < 0) error("ERROR writing to socket");
  get_response(sockfd); 
}

/**
 * Sends the client's list of shared files as a serialized
 * UserFile hash table.
 */
void send_file_list(int sockfd)
{
  int n;
  char *filelist_buf = malloc(sizeof(struct UserFile)*FILELIST_LEN);
  bzero(filelist_buf,sizeof(filelist_buf));
  int bufsize = 0;

  // build serialized hash of all shared files
  build_filelist(&bufsize, filelist_buf);

  // send the SEND_LIST_CMD, followed byt the serialized hash
  // of UserFile structs
  n = write(sockfd,SEND_LIST_CMD,sizeof(SEND_LIST_CMD));
  n = write(sockfd,filelist_buf,bufsize);
  if (n < 0) error("Error writing to socket");   
  get_response(sockfd);
}

/**
 * Threaded process to handle incoming peer requests for file transfers.
 * For each new connection, a unique connection and socket id is created.
 * After the file transfer in complete, the TCP connection is closed and 
 * the thread terminates.
 */
void *handle_xfer_requests(void *my_port)
{ 
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

  // listen for incoming TCP connections on the socket
  listen(sockfd,5);
  
  SELECT_INIT();
 
  for (;;) { 
    SELECT();

    if (s == 0) {
      // awaiting peer connections...
      sleep(1);
    }

    if (s > 0) {
      int n;
      socklen_t clilen;
      int newsockfd;
      struct sockaddr_in cli_addr;
      char buffer[100];
      bzero(buffer,100);
      
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
      
      if (sending == 1) {
        n = send(newsockfd,MSG_BUSY,strlen(MSG_BUSY),0);  
        close(newsockfd);
        continue;
      }
      else {
        // set global var to indicate client is currently
        // busy transferring data
        sending = 1;
        
        // buffer to store incoming message
        char *file_request = malloc(FILENAME_LEN + 1 + USERNAME_LEN);
        n = recv(newsockfd,file_request,FILENAME_LEN + 1 +USERNAME_LEN,0);

        // buffers to store filename and owner name
        char *filename = malloc(FILENAME_LEN);
        char *owner = malloc(USERNAME_LEN);
        filename = strtok(file_request," ");
        owner = strtok(NULL,"\0");
 
        // open file, and send data
        FILE *fp;
        char *filepath = malloc(128);
        char *buf = calloc(MAX_FILE_CHUNK,1);
        int m;
        sprintf(filepath,"%s/%s",SHARED_DIR,filename);
        fp = fopen(filepath,"r");
        m = 1;
        n = 1;
        printf("Sending file %s to client %s\n",filename, owner);
        
        // read in and send up to MAX_FILE_CHUNK per iteration
        while (n > 0 && feof(fp) == 0) {
          m = fread(buf,1,MAX_FILE_CHUNK,fp);
          n = send(newsockfd,buf,m,0);
        }
        
        // file transfer complete, close connection
        close(newsockfd);
        sending = 0;
      }
    }
  }

  close(sockfd);
   
  return 0;
}

/**
 * Handles receiving and displaying of periodic server messages
 */ 
void *handle_incoming_msgs(void *_sockfd)
{
  int sockfd = *(int *)_sockfd;
  SELECT_INIT();
  char *buf = malloc(1024);
  int n = 1;
  for (;;) { // listen for incoming status messages from server
    SELECT();
    if (s > 0) {
      bzero(buf,1024);
      if (expecting_file_info == 1) {
        receive_file_info(sockfd);
        expecting_file_info = 0;
      }
      else {
        n = recv(sockfd,buf,1024,0);
      }
      printf("%s",buf);
      if (n < 1024) {
        printf("\n");
      }
    }
  }
}

/**
 * Threaded process to handle the main loop of user input on the command line.
 */
void *handle_user_input(void *sockfd)
{
  pthread_detach(pthread_self()); //automatically clears the threads memory on exit
  char *user_input = malloc(USER_INPUT_LEN);
  char *command = malloc(COMMAND_LEN);
  char *filename = malloc(FILENAME_LEN);
  int _sockfd = *(int *)sockfd;
  time_t t_now, t_3min;
  time(&t_now);
  t_3min = t_now + THREE_MIN;

  for(;;) {
    // send SendMyFilesList and List commands if 3 minutes has elapsed
    time(&t_now);
    if (t_now > t_3min) {
      send_file_list(_sockfd);
      list_files_available(_sockfd);
    }

    // zero out our input buffers
    bzero(user_input,USER_INPUT_LEN);
    bzero(command,COMMAND_LEN);
    bzero(user_input,FILENAME_LEN);

    // read the user input
    fgets(user_input,100,stdin);
    if(strlen(user_input) > 1) {
      remove_newline(user_input);
      command = strtok(user_input," ");  
    }
    // List
    if (strcmp(command,LIST_CMD) == 0) {
      list_files_available(_sockfd);
      t_3min = t_now + THREE_MIN;
    }
    // Get <filename>
    else if (strcmp(command,GET_CMD) == 0) { 
      filename = strtok(NULL," ");
      get_file(_sockfd, filename);
    }
    // Users
    else if (strcmp(command,USERS_CMD) == 0) {
      user_list(_sockfd);
    }
    // SendMyFilesList
    else if (strcmp(command,SEND_LIST_CMD) == 0) {
      send_file_list(_sockfd);
    }
    // Usage
    else if (strcmp(command,USAGE_CMD) ==0) {
      printf("List\t\t\tList available files.\n");
      printf("Users\t\t\tList active users.\n");
      printf("SendMyFilesList\t\tSend list of shared files to server.\n");
      printf("Get <filename>\t\tGet a file from the network.\n");
    }
    // Exit
    else if (strcmp(command,EXIT_CMD) == 0) {
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
  int sockfd, i, r1, r2, r3;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  pthread_t th1, th2, th3;
  
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
    sprintf(filepath,"%s/%s",SHARED_DIR,argv[i]);
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
    HASH_ADD_INT(my_files, filename, file); // add file to hash table
  }

  // convert port arg to integer
  port = atoi(argv[3]);
  
  // open TCP stream socket 
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");
  
  // validate server IP belongs to a host
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
  
  // request a connection to the server
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    error("ERROR connecting");
  
  // connected, so send over your name to identify yourself
  send_client_name(sockfd,client_name);

  // handle user input
  r1 = pthread_create(&th1, 0, handle_user_input, (void *)&sockfd);
  if (r1 != 0) { fprintf(stderr, "thread create failed\n"); }

  // handle file transfer requests
  r2 = pthread_create(&th2, 0, handle_xfer_requests, (void *)&port);
  if (r2 != 0) { fprintf(stderr, "thread create failed\n"); } 

  //r3 = pthread_create(&th3, 0, handle_incoming_msgs, (void *)&sockfd);
  //if (r3 != 0) { fprintf(stderr, "thread create failed\n"); }

  for (;;) {
    sleep(1); // keep rockin
  }

  return 0;
}
