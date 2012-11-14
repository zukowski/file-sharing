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
#include "my_utils.h"

#define SHARED_DIR "./Shared files"

char client_name[USERNAME_LEN];

// You may/may not use pthread for the client code. The client is communicating with
// the server most of the time until he recieves a "GET <file>" request from another client.
// You can be creative here and design a code similar to the server to handle multiple connections.
void build_filelist(int *bufsize, char *filelist_buf) {
  DIR *dirp;
  struct dirent *entry; 
  FILE *fp;

  dirp = opendir(SHARED_DIR);

  while (1) {
    entry = readdir(dirp);
    if(entry != NULL && strcmp(entry->d_name,"..") != 0 && strcmp(entry->d_name,".") != 0) {
      printf("Filename: %s",entry->d_name);
      struct UserFile *file;
      file = calloc(sizeof(struct UserFile),1); 
      // UserFile.key
      strcpy(file->key,client_name);
      strcat(file->key,entry->d_name);
      
      // UserFile.filename
      strcpy(file->filename,entry->d_name);
     
      // UserFile.filesize
      char *filepath;
      filepath = calloc(FILENAME_LEN*2,1);
      strcpy(filepath,SHARED_DIR);
      strcat(filepath,"/");
      strcat(filepath,entry->d_name);
      fp = fopen(filepath,"r");
      fseek(fp, 0, SEEK_END); // seek to end of file
      int size = ftell(fp); // get current file pointer
      fseek(fp, 0, SEEK_SET); // seek back to beginning of file
      fclose(fp);
      sprintf(file->filesize,"%d",size);
       
      // UserFile.owner
      strcpy(file->owner,client_name);

      // UserFile.owner_ip
      strcpy(file->owner_ip,"127.0.0.1");
      memcpy(filelist_buf,file,sizeof(struct UserFile));
      free(file);
      *bufsize += sizeof(struct UserFile);
    }
    if (entry == NULL)
      break;
  }
  closedir(dirp);
}


int main(int argc, char *argv[])
{
  int sockfd, srv_port, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char msg_buf[256];
  char *filelist_buf;
  filelist_buf = calloc(FILELIST_LEN, sizeof(struct UserFile));

  if (argc < 5) {
    fprintf(stderr,"usage: %s <client name> <server ip> <server port #> <list of files from “shared files directory”>\n", argv[0]);
    exit(0);
  }
 
  // load args
  strcpy(client_name,argv[1]);

  srv_port = atoi(argv[3]);
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");
  
  server = gethostbyname(argv[2]);
  
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
    
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
  serv_addr.sin_port = htons(srv_port);
  
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    error("ERROR connecting");
  
  printf("> ");
  bzero(filelist_buf,sizeof(filelist_buf));
  int bufsize = 0;
  build_filelist(&bufsize, filelist_buf); 
  
  n = write(sockfd,filelist_buf,bufsize);
  if (n < 0) error("ERROR writing to socket");
  
  printf("Wrote %d bytes to socket", bufsize); 
  
  bzero(msg_buf,sizeof(msg_buf));
  n = read(sockfd,msg_buf,255);
  if (n < 0) error("ERROR reading from socket");
    
  printf("%s\n",msg_buf);
  close(sockfd);
  
  return 0;
}
