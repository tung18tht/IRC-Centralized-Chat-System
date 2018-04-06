#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>

#define MAX_CLIENT 100

int main() {
  int sockfd, fl, clen, clientfd;
  int client_to_server_pipe[MAX_CLIENT][2] = {0};
  int server_to_client_pipe[MAX_CLIENT][2] = {0};
  struct sockaddr_in saddr, caddr;
  unsigned short port = 8784;
  
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
      
      fl = fcntl(clientfd, F_GETFL, 0);
      fl |= O_NONBLOCK;
      fcntl(sockfd, F_SETFL, fl);
      
      int i;
      for (i = 0; i < MAX_CLIENT; i++) {
        if ((server_to_client_pipe[i][0] == 0) && (server_to_client_pipe[i][1] == 0) && (client_to_server_pipe[i][0] == 0) && (client_to_server_pipe[i][1] == 0)){
          pipe (server_to_client_pipe[i]);
          pipe (client_to_server_pipe[i]);
          break;
        }
      }
      
      switch(fork()) {
        case -1:
          printf("Cannot create process!\n");
          return 1;
        case 0:
          close(server_to_client_pipe[i][1]);
          close(client_to_server_pipe[i][0]);
          while(1) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(server_to_client_pipe[i][0], &set);
            
            select(server_to_client_pipe[i][0]+1, &set, NULL, NULL, NULL);
            
            if (FD_ISSET(server_to_client_pipe[i][0], &set)) {
              char message[1024];
              read(server_to_client_pipe[i][0], message, sizeof(message));
              
              printf("Message received: %s\n", message);
            }
          }
      }
    }
  }
}