#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>

int sockfd, clen, clientfd, clientfds[100], fl;
struct sockaddr_in saddr, caddr;
unsigned short port = 8784;
char message[256];

void *write_to_client() {
  while(1) {
    printf("Server: ");
    scanf("%s", message);

    for(int i=0; i<100; i++) {
      if (clientfds[i] > 0) {
        write(clientfds[i], message, sizeof(message));
      }
    }
  }
}

int main() {
  memset(clientfds, 0, sizeof(clientfds));

  if ((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Error creating socket\n");
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
    printf("Error binding\n");
  }

  if (listen(sockfd, 5) < 0) {
    printf("Error listening\n");
  }

  pthread_t write_to_client_thread;
  pthread_create(&write_to_client_thread, NULL, write_to_client, NULL);

  while(1) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    int maxfd = sockfd;

    for(int i=0; i<100; i++) {
      if (clientfds[i] > 0) {
        FD_SET(clientfds[i], &set);
      }
      if (clientfds[i] > maxfd) {
        maxfd = clientfds[i];
      }
    }

    select(maxfd+1, &set, NULL, NULL, NULL);

    if (FD_ISSET(sockfd, &set)) {
      clen=sizeof(caddr);
      clientfd = accept(sockfd, (struct sockaddr *) &caddr, &clen);

      fl = fcntl(clientfd, F_GETFL, 0);
      fl |= O_NONBLOCK;
      fcntl(sockfd, F_SETFL, fl);

      for (int i=0; i<100; i++) {
        if (clientfds[i] == 0) {
          clientfds[i] = clientfd;
          break;
        }
      }
    }

    for (int i=0; i<100; i++) {
      if (clientfds[i]>0 && FD_ISSET(clientfds[i], &set)) {
        if (read(clientfds[i], message, sizeof(message)) > 0) {
          printf("Client %d: %s\n", clientfds[i], message);
        } else {
          printf("Client %d disconnected\n", clientfds[i]);
          clientfds[i] = 0;
        }
      }
    }
  }
}