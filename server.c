/*
  Author: Daniel Zukowski <daniel.zukowski@gmail.com>
  Date: Nov 16, 2012
 
  A basic TCP file transfer server.
  This server doesn't actually store any files itself, but rather
  serves as the central communication node throuhg which clients gain
  information about files available across the network.
  The server listens for incoming client connections and spawns a thread
  for each new client connection using pthreads.

  The server maintains a hash table of all files available on the network.
  The hash table data structure itself is provided by the uthash project:
  http://uthash.sourceforge.net/

  Constants and Macros defined in app_utils.h
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


//------------------------------------------------
// Global vars                                    
//------------------------------------------------

// a threaded function for handling client connections
void *connection(void *);

// from argv: the name of the log file
char log_filename[FILENAME_LEN];

// hash table of connected users
struct User *users = NULL;

// hash table of files offered for sharing
struct UserFile *user_files = NULL; 

// mutex for atomic actions
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


//------------------------------------------------
// Functions                                    
//------------------------------------------------


/**
 * Adds a User structure to the global users hash table
 * if its key (i.e. it's name) does not yet exist
 */
void add_user(struct User *user) {
  struct User *u;
  int exists = 0;
  for (u = users; u != NULL; u = u->hh.next) {
    if (strcmp(u->name,user->name) == 0) {
      exists = 1;
      break; 
    }	
  }
  if (exists == 0) {
    HASH_ADD_INT(users, name, user);
    char *message;
    message = calloc(LOG_ENTRY_LEN,1);
    _log(log_filename,message);
    free(message);
  }
}

/**
 * Removes a User structure from the global users hash table
 */
void remove_user(struct User *user) {
  // TODO
}

/**
 * Provides basic TCP stream socket set up and binding.
 * Returns the socket descriptor created. 
 */
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

/**
 * Adds a UserFile struct to the global user_files hash table.
 */
void add_file(struct UserFile *user_file) {
  int exists = 0;
  struct UserFile *f = malloc(sizeof(struct UserFile));
  
  // iterate over hash table to find if key exists
  for(f = user_files; f != NULL; f = f->hh.next) {
    if (strcmp(f->filename,user_file->filename) == 0) {
      exists = 1;
      break;
    }
  }
  if (exists == 0) {
    // add UserFile to hash table
    HASH_ADD_INT(user_files, filename, user_file); 
  }
}

/**
 * Receives and processes a list of shared files from a client.
 */
void receive_file_list(int sockfd, struct User *user) {
  int i, n, num_files;
  char *buffer = malloc(sizeof(struct UserFile)*FILELIST_LEN);
  n = recv(sockfd,buffer,sizeof(struct UserFile)*FILELIST_LEN,0);

  // determine how many files have been received by dividing the size of the
  // byte stream by the size of a UserFile struct
  num_files = n / sizeof(struct UserFile);

  // now add the UserFiles to the hash table
  for (i = 0; i < num_files; i++) {
    struct UserFile *file = malloc(sizeof(struct UserFile));
    memcpy(file,buffer,sizeof(struct UserFile));
    buffer += sizeof(struct UserFile);
    // special handling of the IP address field
    inet_ntop(AF_INET,&(user->addr.sin_addr.s_addr),file->owner_ip,OWNER_IP_LEN);
    add_file(file);
  }

  // Always thank your client
  n = send(sockfd,"Thank you for your list",23,0);
}

/**
 * Builds a formatted table of file information
 */
void build_file_list(char *filelist_buf) {
  strcat(filelist_buf,"File name \t|| File size \t|| File owner \t|| File owner's IP address\n\n");
  struct UserFile *f;
  for(f = user_files; f != NULL; f = f->hh.next) {
    char fileinfo[256];
    sprintf(fileinfo,"%s \t|| %s \t || %s \t || %s \t\n",f->filename,f->filesize,f->owner,f->owner_ip);
    strcat(filelist_buf,fileinfo);
  }  
}

/**
 * Send the list of currently available user files to the client
 */
void send_file_list(int sockfd) {
  int n;
  char *filelist_buf = malloc(sizeof(struct UserFile)*FILELIST_LEN);
  build_file_list(filelist_buf);
  n = send(sockfd,filelist_buf,strlen(filelist_buf),0);
  if (n < 0) error("ERROR writing to socket");
}

/**
 * Send information about a specific file upon client request.
 */
void send_file_info(int sockfd, char *filename) {
  int n;
  struct UserFile *file = NULL;
  for (file = user_files; file != NULL; file = file->hh.next) {
    if (strcmp(file->filename,filename) == 0) {
      char *buf = malloc(sizeof(struct UserFile));
      printf("Found file: %s\n",file->filename);
      memcpy(buf,file,sizeof(struct UserFile));
      n = send(sockfd,buf,sizeof(struct UserFile),0);
      if (n < 0) error("ERROR writing to socket");
      break;
    }
  }
  if (file == NULL) {
    char *buf = malloc(64);
    strcpy(buf,MSG_FILE_NA);
    n = send(sockfd,buf,strlen(MSG_FILE_NA),0);
    if (n < 0) error("ERROR writing to socket");
  }
}

/**
 * Send a list of connected users to the client.
 */
void send_user_list(int sockfd) {
  int n;
  char *userlist_buf = malloc(1024);
  strcat(userlist_buf,"Currently connected users:\n");
  struct User *u;
  for(u = users; u != NULL; u = u->hh.next) {
    char *userinfo = calloc(512,1);
    sprintf(userinfo,"%s\n",u->name);
    strcat(userlist_buf,userinfo);
  }
  n = send(sockfd,userlist_buf,strlen(userlist_buf),0); 
  if (n < 0) error("ERROR writing to socket");
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
  
  SELECT_INIT();
  for ( ; ; ) //endless loop
  {
    SELECT(); 
    if(s == 0) {
      // timeout, do nothing
    }
	  
    if (s > 0) { // something ready for read, probably a client connection request
      int n, r;
      socklen_t clilen;
      int newsockfd;
      struct sockaddr_in cli_addr;
      char buffer[100];
      char welcome_msg[100];
      pthread_t th;

      clilen = sizeof(cli_addr);
      
      // accept connection
      newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

      if (newsockfd < 0) error("ERROR on accept");
      FD_SET(newsockfd, &rdset);
      max_fd = newsockfd + 1;

      // check select to see if client is sending their username
      s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 
      if (s == -1) { printf("ERROR: Socket error. Exiting.\n"); exit(1); }

      if (s > 0) {
        bzero(buffer,100);
        n = recv(newsockfd,buffer,100,0);
        if (n < 0) error("ERROR reading from socket");
    
        // since a new user connected, let's review the current file list
        char *filelist_buf = malloc(sizeof(struct UserFile)*FILELIST_LEN);
        build_file_list(filelist_buf);
        printf("%s just started connecting. This is the updated file list from all clients:\n%s\n",buffer,filelist_buf); 
        
        // add user to users hash
        pthread_mutex_lock(&mutex);
          struct User *user = malloc(sizeof(struct User));
          strcpy(user->name,buffer);
          memcpy(&(user->addr),&cli_addr,sizeof(struct sockaddr_in));
          user->sockfd = newsockfd;
          add_user(user);
	      pthread_mutex_unlock(&mutex);

        // send welcome message
        strcpy(welcome_msg,"Welcome, ");
        strcat(welcome_msg, buffer);
        strcat(welcome_msg,". You are now connected.");
        n = send(newsockfd,welcome_msg,strlen(welcome_msg),0); 

        // notify other users that a new user has joined
        /*
        struct User *u;
        char *new_user_msg = malloc(128);
        strcpy(new_user_msg,"New user connected: ");
        for(u = users; u != NULL; u = u->hh.next) {
          if (strcmp(u->name,buffer) != 0) {
            sprintf(new_user_msg,"A new user has joined: %s\n",buffer);
            n = send(u->sockfd,new_user_msg,strlen(new_user_msg),0);
          }
        }
        */
        r = pthread_create(&th, 0, connection, (void *)user);
        if (r != 0) { fprintf(stderr, "thread create failed\n"); }
      }

      if (s == 0) {
        printf("Client did not send username\n");
      }
    }
  }
 
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

//-----------------------------------------------------------------------------
void *connection(void *user) {
  //struct timeval curTime;
  struct User *_user = (struct User *)user;
  int e;
  int rc = 1;
  int sockfd = _user->sockfd;
  pthread_detach(pthread_self()); //automatically clears the threads memory on exit
  fd_set rdset, wrset;
  int s = -1;
  int max_fd = sockfd + 1;
  struct timeval selectTimeout;
  
  selectTimeout.tv_sec = TIMEOUT / 1000;
  selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000;
  FD_ZERO(&rdset);
  FD_ZERO(&wrset);
  
  char *buf = calloc(100,1);
  for (;;) {
    // start monitoring for reads or timeout
    if (s <= 0) {
      FD_SET(sockfd, &rdset);
    }

    s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); 

    if (s == -1) { printf("ERROR: Socket error. Exiting.\n"); exit(1); }

    /*
     * Here we handle all of the incoming text from a particular client.
     */
    if(s > 0) {
      bzero(buf,100);
      rc = recv(sockfd,buf,99,0);
      if (rc > 0)
      {
        char *cmd = malloc(24);
        cmd = strtok(buf," ");
 
        // which command are we processing? 
        if (strcmp(cmd,GET_CMD) == 0) {
          char *filename = calloc(FILENAME_LEN,1);
          filename = strtok(NULL,"\0");
          send_file_info(sockfd, filename);
        }
        else if (strcmp(cmd,LIST_CMD) == 0) {
          send_file_list(sockfd);
        }
        else if (strcmp(cmd,USERS_CMD) == 0) {
          send_user_list(sockfd);
        }
        else if (strcmp(cmd,SEND_LIST_CMD) == 0) {
          receive_file_list(sockfd,_user);
        }
        else if (strcmp(cmd,EXIT_CMD) == 0) {
          //if I received an 'exit' message from this client
	        pthread_mutex_lock(&mutex);
	        remove_user(_user);
	        pthread_mutex_unlock(&mutex);
	        pthread_exit(NULL);
	        printf("Shouldn't see this!\n");
          //loop through global client list and
	        //e =write(..); 
	        if (e == -1) //broken pipe.. someone has left the room
	        {
	          pthread_mutex_lock(&mutex);
	          //so remove them from our list
	          pthread_mutex_unlock(&mutex);
	        }
        }
        else {
          printf("Unknown command received: %s\n",cmd);
        }
        fflush(stdout);
      }
    }
  }	
	//A requirement for 5273 students:
  //if I received a new files list from this client, the
	//server must “Push”/send the new updated hash table to all clients it is connected to.

  //should probably never get here
  return 0;
}
