#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>

int main() {
  int sockfd, fl, clen, clientfd, client_pipe_fds[100][2];
  struct sockaddr_in saddr, caddr;
  unsigned short port = 8784;
  
  for (int i = 0; i < 100; i++) {
    client_pipe_fds[i][0] = client_pipe_fds[i][1] = 0;
  }
  
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
    FD_SET(0, &set);
    
    select(sockfd+1, &set, NULL, NULL, NULL);

    if (FD_ISSET(sockfd, &set)) {
      clen=sizeof(caddr);
      clientfd = accept(sockfd, (struct sockaddr *) &caddr, &clen);
      int i;
      for (i = 0; i < 100; i++) {
        if ((client_pipe_fds[i][0] == 0) && (client_pipe_fds[i][1] == 0)){
          pipe (client_pipe_fds[i]);
          break;
        }
      }
      
      switch(fork()) {
        case -1:
          printf("Cannot create process!\n");
          return 1;
        case 0:
          close(client_pipe_fds[i][1]);
          fflush(stdout);
          while(1) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(client_pipe_fds[i][0], &set);
            
            select(client_pipe_fds[i][0]+1, &set, NULL, NULL, NULL);
            
            if (FD_ISSET(client_pipe_fds[i][0], &set)) {
              char message[1024];
              read(client_pipe_fds[i][0], message, sizeof(message));
              
              printf("Message received: %s\n", message);
            }
          }
          break;
        default:
          // close(client_pipe_fds[i][0]);
          break;
      }
    }
    
    if (FD_ISSET(0, &set)) {
      char message[1024];
      fgets(message, 1024, stdin);
      if ((strlen(message) > 0) && (message[strlen (message) - 1] == '\n')) {
        message[strlen (message) - 1] = '\0';
      }
  
      for(int i=0; i<100; i++) {
        if ((client_pipe_fds[i][0] != 0) && (client_pipe_fds[i][1] != 0)){
          write(client_pipe_fds[i][1], message, sizeof(message));
        }
      }
    }
  }
}