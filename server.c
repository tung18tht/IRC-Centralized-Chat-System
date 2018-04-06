#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>

int sockfd, fl, clen, cientfd, client_pipe_fds[100][2];
struct sockaddr_in saddr;
unsigned short port = 8784;

int main() {
  if ((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Error creating socket!\n");
    return 1;
  }

  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  fl = fcntl(sockfd, F_GETFL, 0);
  fl |= O_NONBLOCK;
  fcntl(sockfd, F_SETFL, fl);

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
    printf("Error binding!\n");
    return 1;
  }

  if (listen(sockfd, 10) < 0) {
    printf("Error listening!\n");
    return 1;
  }

  printf("Server created, started listening for connections...\n");

  while(1) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    
    select(sockfd+1, &set, NULL, NULL, NULL);

    if (FD_ISSET(sockfd, &set)) {
      clen=sizeof(caddr);
      clientfd = accept(sockfd, (struct sockaddr *) &caddr, &clen);
      
      switch(fork()) {
        case -1:
          printf("Cannot create process!\n");
          return 1;
        case 0:
          ...;
          break;
        default:
          ...;
      }
    }
  }
}