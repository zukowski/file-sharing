#include<stdlib.h>
#include<unistd.h>

int main() {
  system("./server 50000 server.log");   
  sleep(1);
  system("./client ubuntu 127.0.0.1 50000 file.txt");
  return 0;
}
