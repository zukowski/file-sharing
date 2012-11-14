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
// global variables 
char now[DATETIME_LEN];

void error(const char *msg) {
  perror(msg);
  exit(1);
}

void _now() {
  time_t currentTime = time(NULL);
  struct tm timeStruct;
  localtime_r(&currentTime, &timeStruct);
  strftime(now, DATETIME_LEN, "%Y-%m-%d %H:%M:%S", &timeStruct);
}

void _log(const char *logFileName, char *message) {
  FILE *logFile = fopen(logFileName,"a");
  _now();
  fprintf(logFile,"%s %s\n",now,message);
  fclose(logFile);
}

