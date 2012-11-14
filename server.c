/*
 * Skeleton code of a server using TCP protocol.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "my_utils.h"
#define MAX_CONNECTS 50
#define TCP_PROTO 6
#define TIMEOUT 50

/**
 * [SYN HEADER]
 * offset            data
 * -----------------------------
 *
 *
 * [SYN BODY]   
 * offset  length         data 
 * -----------------------------
 * 0       USERNAME_LEN   client name
 *            
 */

/*
 * You should use a globally declared linked list or an array to 
 * keep track of your connections. Be sure to manage either properly
 */

// global variables
void *connection(void *);
char log_filename[FILENAME_LEN];
struct User *users = NULL;
struct UserFile *user_files = NULL; 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void init_user_struct(struct User *u) {
  bzero(&(u->name),USERNAME_LEN);
  bzero(&(u->files),FILELIST_LEN*FILENAME_LEN);
}

char *get_username(char *buf) {
  return "ubuntu";    
}

void add_user(char *buf) {
  struct User *user;
  HASH_FIND_INT(users, get_username(buf), user);
  if (user == NULL) {
    user = calloc(1,sizeof(struct User));
    strcpy(user->name,buf);
    HASH_ADD_INT(users, name, user);
    char *message;
    message = calloc(LOG_ENTRY_LEN,1);
    sprintf(message,"User connected: %s",user->name);
    _log(log_filename,message);
    free(message);
  }
}

void add_file(struct UserFile *user_file) {
  struct UserFile *file;
  HASH_FIND_INT(user_files, user_file->key, file); 
  if (file != NULL) { // update entry in case size or IP changed
    strcpy(file->filesize,user_file->filesize);
    strcpy(file->owner_ip,user_file->owner_ip);
  } 
  else { // add entry
    file = calloc(1,sizeof(struct UserFile));
    memcpy(file, user_file, sizeof(struct UserFile));
    HASH_ADD_INT(user_files, key, file); 
  }
}

int setup_socket(int port) {
  int sockfd;
  struct sockaddr_in serv_addr;
     
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);
  
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    error("ERROR on binding");
  }
  
  return sockfd;
}

void accept_connection(int sockfd) {
  int n, r;
  socklen_t clilen;
  int newsockfd;
  struct sockaddr_in cli_addr;
  char buffer[1000];
  pthread_t th;
  void *res;

  clilen = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if (sockfd < 0) error("ERROR on accept");

  bzero(buffer,1000);
  n = read(newsockfd,buffer,999);
  if (n < 0) error("ERROR reading from socket");

  //add_user(buffer);
   
  pthread_mutex_lock(&mutex);
	  // TODO: add your new user to your global list of users
	pthread_mutex_unlock(&mutex);
  
  r = pthread_create(&th, 0, connection, (void *)newsockfd);
	if (r != 0) { fprintf(stderr, "thread create failed\n"); }
  
  // convert buffer into UserFile struct
  struct UserFile *file;
  file = calloc(1,sizeof(struct UserFile));
  memcpy(file,buffer,sizeof(struct UserFile));  
  add_file(file);
 
  printf("\nFILE INFO\nKey: %s\nFilename: %s\nSize: %s\nOwner: %s\nOwner IP: %s\n",
         file->key,
         file->filename,
         file->filesize,
         file->owner,
         file->owner_ip);

  n = write(newsockfd,"Thanks for your list",21);
  if (n < 0) error("ERROR writing to socket");
  
  close(newsockfd);
  
  pthread_join(th, &res);
}

void listen_for_connections(int sockfd) {
  fd_set rdset, wrset;
  int s = -1;
  int max_fd = sockfd + 1;
  struct timeval selectTimeout;
  
  selectTimeout.tv_sec = TIMEOUT / 1000;
  selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000;

  FD_ZERO(&rdset);
  FD_ZERO(&wrset);

  // start monitoring for reads or timeout
  if (s <= 0) FD_SET(sockfd, &rdset);
  
  s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 
  if (s == -1) { printf("ERROR: Socket error. Exiting.\n"); exit(1); }

  if (s > 0) {
    accept_connection(sockfd);
  }
}

int main(int argc,char *argv[])
{
     
  struct timeval currTime;
  int sockfd;
  int port;

  //check arguments here
  if (argc != 3) {
	  printf("usage: %s <port#> <logFile>\n", argv[0]);
	  return 0;
  }
  
  // set port number var
  port = atoi(argv[1]);
  
  // copy log file name into log_filename
  memcpy(log_filename,argv[2],strlen(argv[2]));
  
  // log the start of the server
  gettimeofday(&currTime,NULL);
  _log(log_filename,"Server started");
  
  /*
   * Open a TCP socket (an Internet stream socket).
   */
  sockfd = setup_socket(port);
  
  /*
   * here you should listen() on your TCP socket
   */
  listen(sockfd,5);

  for ( ; ; ) //endless loop
  {
    listen_for_connections(sockfd);
	//now that you're listening, check to see if anyone is trying 
	//to connect
	//hint: select()
  //s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 

	//if someone is trying to connect, you'll have to accept() 
	//the connection
    //newsockfd = accept(...)
    //accept_connection();
    	//if you've accepted the connection, you'll probably want to
	//check "select()" to see if they're trying to send data, 
    //like their name, and if so
	//recv() whatever they're trying to send

	//since you're talking nicely now.. probably a good idea send them
	//a message to welcome them to the new client.

	//if there are others connected to the server, probably good to notify them
	//that someone else has joined.


	//pthread_mutex_lock(&mutex);
	//now add your new user to your global list of users
	//pthread_mutex_unlock(&mutex);

	//now you need to start a thread to take care of the 
	//rest of the messages for that client
	//r = pthread_create(&th, 0, connection, (void *)newsockfd);
	//if (r != 0) { fprintf(stderr, "thread create failed\n"); }

	//A requirement for 5273 students:
	//at this point...
	//whether or not someone connected, you should probably
	//look for clients that should be timed out
	//and kick them out
	//oh, and notify everyone that they're gone.


  }
}





//-----------------------------------------------------------------------------
void *connection(void *sockid) {
  //int s = (int)sockid;

  //char buffer[1000];
  //struct timeval curTime;
  int e;
  int rc = 0;

  pthread_detach(pthread_self()); //automatically clears the threads memory on exit


  /*
   * Here we handle all of the incoming text from a particular client.
   */

  //rc = recv()
  while (rc > 0)
  {
	  //if I received an 'exit' message from this client
	  pthread_mutex_lock(&mutex);
	  //remove myself from the vector of active clients
	  pthread_mutex_unlock(&mutex);
	  pthread_exit(NULL);
	  printf("Shouldn't see this!\n");
	
	
	  //A requirement for 5273 students:
    //if I received a new files list from this client, the
	  //server must “Push”/send the new updated hash table to all clients it is connected to.
	
	
	  //loop through global client list and
	  //e =write(..); 
	  if (e == -1) //broken pipe.. someone has left the room
	  {
	    pthread_mutex_lock(&mutex);
	    //so remove them from our list
	    pthread_mutex_unlock(&mutex);
	  }
  }

  //should probably never get here
  return 0;
}
