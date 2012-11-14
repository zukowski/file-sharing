/*
 * Skeleton code of a server using TCP protocol.
 */

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
#include <stdlib.h>


#define MAX_CONNECTS 50

/*
 * You should use a globally declared linked list or an array to 
 * keep track of your connections. Be sure to manage either properly
 */

//thread function declaration
void *connection(void *);

//global variables
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char logFileName[64];



int main(int argc,char *argv[])
{
  
  struct timeval currTime;
  pthread_t th;
  int r;
  FILE *logFile;
  int newsockfd = 0;


  //check arguments here
  if (argc != 3) {
	  printf("usage is: ./pserver <port#> <logFile>\n");
	  return 0;
  }

  gettimeofday(&currTime,NULL);
  logFile = fopen(logFileName,"w");
  //fprintf(logFile,"Server started at ...");
  fclose(logFile);

  /*
   * Open a TCP socket (an Internet stream socket).
   */


  /*
   * Bind our local address so that the client can send to us.
   */


  /*
   * here you should listen() on your TCP socket
   */

  for ( ; ; ) //endless loop
  {
  

	//now that you're listening, check to see if anyone is trying 
	//to connect
	//hint: select()

	//if someone is trying to connect, you'll have to accept() 
	//the connection
    //newsockfd = accept(...)

	//if you've accepted the connection, you'll probably want to
	//check "select()" to see if they're trying to send data, 
    //like their name, and if so
	//recv() whatever they're trying to send

	//since you're talking nicely now.. probably a good idea send them
	//a message to welcome them to the new client.

	//if there are others connected to the server, probably good to notify them
	//that someone else has joined.


	pthread_mutex_lock(&mutex);
	//now add your new user to your global list of users
	pthread_mutex_unlock(&mutex);

	//now you need to start a thread to take care of the 
	//rest of the messages for that client
	r = pthread_create(&th, 0, connection, (void *)newsockfd);
	if (r != 0) { fprintf(stderr, "thread create failed\n"); }

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
  int s = (int)sockid;

  char buffer[1000];
  struct timeval curTime;
  int e, rc = 0;

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
