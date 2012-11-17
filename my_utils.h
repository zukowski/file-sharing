#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "uthash-1.9.7/src/uthash.h"

// sizes for memory allocation
#define DATETIME_LEN 23
#define FILELIST_LEN 128
#define FILENAME_LEN 64
#define FILESIZE_LEN 10
#define USERNAME_LEN 64
#define LOG_ENTRY_LEN 256
#define OWNER_IP_LEN 16
#define COMMAND_LEN 16
#define USER_INPUT_LEN 256

// commands & messages
#define LIST_CMD "List"
#define GET_CMD "Get" 
#define USERS_CMD "Users"
#define SEND_LIST_CMD "SendMyFilesList"
#define USAGE_CMD "Usage"
#define EXIT_CMD "Exit"
#define MSG_BUSY "The machine hosting this file is not available at this moment, please wait a little and try another time or get the file from another machine"
#define MSG_FILE_NA "File requested is not available"
#define MSG_EMPTY "No message in server response"
#define RECV_ERR "Error reading from socket" 

// timer values for checking for 
#define THREE_MIN 180
#define FIVE_MIN 300

#define SHARED_DIR "./Shared files"
#define DL_DIR "./Downloaded files"
#define MAX_FILE_CHUNK 10490000 // 10MB, doubtful it'll ever get this high though
#define TIMEOUT 1000
#define MAX_TIMEOUTS 5

//*************************************************************
// Macros
//*************************************************************

#define SELECT_INIT() \
  fd_set rdset, wrset; \
  int s = -1; \
  int max_fd = sockfd + 1; \
  struct timeval selectTimeout;

#define SELECT() \
  selectTimeout.tv_sec = TIMEOUT / 1000; \
  selectTimeout.tv_usec = (TIMEOUT % 1000) * 1000; \
  FD_ZERO(&rdset); \
  FD_ZERO(&wrset); \
  if (s <= 0) \
    FD_SET(sockfd, &rdset); \
  s = select(max_fd, &rdset, &wrset, NULL, &selectTimeout); \
  if (s == -1) { \
    printf("ERROR: Socket error. Exiting.\n"); \
    exit(1); \
  }

//*************************************************************
// Structures
//*************************************************************

struct User {
  char name[USERNAME_LEN];      // hask key
  int sockfd;
  struct sockaddr_in addr;
  UT_hash_handle hh;            // makes this struct hashable
};

struct UserFile {
  char filename[FILENAME_LEN];
  char filesize[FILESIZE_LEN];
  char owner[USERNAME_LEN];
  char owner_ip[OWNER_IP_LEN];
  UT_hash_handle hh;            // makes this struct hashable
};


//*************************************************************
// Functions
//*************************************************************

// Reports and exits on error.
void error(const char *msg);

// Generates human-readable current time.
void _now();

// Writes a log entry, prepended with the current datetime.
void _log(const char *logFileName, char *message);

// Strips the newline character from stdin input retrieved with fgets
char *remove_newline(char *s);

void build_filelist(int *bufsize, char *filelist_buf);
