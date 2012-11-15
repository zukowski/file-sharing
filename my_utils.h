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

#define DATETIME_LEN 23
#define FILELIST_LEN 128
#define FILENAME_LEN 64
#define FILESIZE_LEN 6
#define USERNAME_LEN 64
#define LOG_ENTRY_LEN 256
#define OWNER_IP_LEN 16
#define HEADER_LEN 32

#define COMMAND_LEN 16
#define LIST_CMD "List"
#define GET_CMD "Get" 
#define SEND_LIST_CMD "SendMyFilesList"
#define EXIT_CMD "Exit"

// structures
struct User {
  char name[USERNAME_LEN];                /* key */
  UT_hash_handle hh;                      /* makes this structure hashable */
};

struct UserFile {
  char key[USERNAME_LEN + FILENAME_LEN];
  char filename[FILENAME_LEN];
  char filesize[FILESIZE_LEN];
  char owner[USERNAME_LEN];
  char owner_ip[OWNER_IP_LEN];
  UT_hash_handle hh;                      /* makes this structure hashable */
};

//Reports and exits on error.
void error(const char *msg);

//Generates human-readable current time.
void _now();

//Writes a log entry, prepended with the current datetime.
void _log(const char *logFileName, char *message);
