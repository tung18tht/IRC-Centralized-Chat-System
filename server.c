#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>

#define MAX_CLIENT 100
#define BUFFER_SIZE 1024

int sockfd, fl, clen, clientfd;
int child_to_parent_pipe[MAX_CLIENT][2] = {0};
int parent_to_child_pipe[MAX_CLIENT][2] = {0};
struct sockaddr_in saddr, caddr;
unsigned short port = 8784;

void disconnect(int socket) {
  shutdown(socket, SHUT_RDWR);
  close(socket);
}

void clean_pipe(int client) {
  close(parent_to_child_pipe[client][1]);
  close(child_to_parent_pipe[client][0]);
  child_to_parent_pipe[client][0] = 0;
  child_to_parent_pipe[client][1] = 0;
  parent_to_child_pipe[client][0] = 0;
  parent_to_child_pipe[client][1] = 0;
}

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

  printf("Server created, listening for connections...\n");

  while(1) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    
    int maxfd = sockfd;
    for(int i=0; i<MAX_CLIENT; i++) {
      if (child_to_parent_pipe[i][0] > 0) {
        FD_SET(child_to_parent_pipe[i][0], &set);
        if (child_to_parent_pipe[i][0] > maxfd) {
          maxfd = child_to_parent_pipe[i][0];
        }
      }
    }
    
    select(maxfd+1, &set, NULL, NULL, NULL);

    if (FD_ISSET(sockfd, &set)) {
      clen=sizeof(caddr);
      clientfd = accept(sockfd, (struct sockaddr *) &caddr, &clen);
      
      fl = fcntl(clientfd, F_GETFL, 0);
      fl |= O_NONBLOCK;
      fcntl(sockfd, F_SETFL, fl);
      
      int i;
      for (i = 0; i < MAX_CLIENT; i++) {
        if ((parent_to_child_pipe[i][0] == 0) && (parent_to_child_pipe[i][1] == 0) && (child_to_parent_pipe[i][0] == 0) && (child_to_parent_pipe[i][1] == 0)){
          pipe (parent_to_child_pipe[i]);
          pipe (child_to_parent_pipe[i]);
          printf("Client %d connected\n", i);
          break;
        }
      }
      
      switch(fork()) {
        case -1:
          printf("Cannot create process!\n");
          return 1;
        case 0:
          close(parent_to_child_pipe[i][1]);
          close(child_to_parent_pipe[i][0]);
          while(1) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(parent_to_child_pipe[i][0], &set);
            FD_SET(clientfd, &set);
            
            int maxfd = clientfd > parent_to_child_pipe[i][0] ? clientfd : parent_to_child_pipe[i][0];
            
            select(maxfd + 1, &set, NULL, NULL, NULL);
            
            if (FD_ISSET(parent_to_child_pipe[i][0], &set)) {
              char message[BUFFER_SIZE];
              if(read(parent_to_child_pipe[i][0], message, sizeof(message)) > 0) {
                write(clientfd, message, sizeof(message));
              } else {
                return 1;
              }
            }
            
            if(FD_ISSET(clientfd, &set)){
              char message[BUFFER_SIZE];
              if(read(clientfd, message, sizeof(message)) > 0) {
                write(child_to_parent_pipe[i][1], message, sizeof(message));
                if(strcmp(message, "/quit") == 0) {
                  disconnect(clientfd);
                  close(parent_to_child_pipe[i][0]);
                  close(child_to_parent_pipe[i][1]);
                  return 0;
                }
              } else {
                sprintf(message, "/quit");
                write(child_to_parent_pipe[i][1], message, sizeof(message));
                disconnect(clientfd);
                close(parent_to_child_pipe[i][0]);
                close(child_to_parent_pipe[i][1]);
                return 1;
              }
            }
          }
          break;
        default:
          close(parent_to_child_pipe[i][0]);
          close(child_to_parent_pipe[i][1]);
      }
    }
    
    for (int i=0; i<MAX_CLIENT; i++) {
      if (child_to_parent_pipe[i][0] > 0 && FD_ISSET(child_to_parent_pipe[i][0], &set)) {
        char message[BUFFER_SIZE];
        if(read(child_to_parent_pipe[i][0], message, sizeof(message)) < 0){
          return 1;
        }
        if(strcmp(message, "/quit") == 0) {
          clean_pipe(i);
          printf("Client %d disconnected\n", i);
          continue;
        }
        for (int j=0; j<MAX_CLIENT; j++) {
          if (j == i) {
            continue;
          }
          if(parent_to_child_pipe[j][1] > 0) {
            write(parent_to_child_pipe[j][1], message, sizeof(message));
          }
        }
      }
    }
  }
}