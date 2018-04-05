#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct sockaddr_in saddr;
struct in_addr *addr;
struct hostent *host;
int sockfd, fl;
unsigned short port = 8784;

void *input_handler() {
  char message[1024];
  while(1) {
    printf("You: ");
    fgets(message, sizeof(message), stdin);
    if (strlen(message) == 1) {
      continue;
    }
    if ((strlen(message) > 0) && (message[strlen (message) - 1] == '\n')) {
      message[strlen (message) - 1] = '\0';
    }
    if (strcmp(message, "/quit") == 0) {
      printf("Program exit...\n");
      cleanup_and_exit(0);
    }
    write(pipefds[1], message, sizeof(message));
  }
}

void *network_handler() {
  char message[1024];
  while(1){
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    FD_SET(pipefds[0], &set);

    select(sockfd>pipefds[0] ? sockfd+1:pipefds[0]+1, &set, NULL, NULL, NULL);

    if (FD_ISSET(sockfd, &set)) {
      if(read(sockfd, message, sizeof(message)) > 0) {
        printf("\n%s\nYou: ", message);
        fflush(stdout);
      } else {
        printf("\nServer closed\n");
        cleanup_and_exit(1);
      }
    }
    
    if (FD_ISSET(pipefds[0], &set)) {
      if(read(pipefds[0], message, sizeof(message)) > 0) {
        write(sockfd, message, sizeof(message));
      } else {
        cleanup_and_exit(1);
      }
    }
  }
}

int main(int argc, char **argv) {
  if(argc == 2) {
    host = gethostbyname(argv[1]);
  } else {
    printf("Enter your server domain: ");
    char domain[100];
    scanf("%s", domain);
    host = gethostbyname(domain);
  }

  if (host == NULL) {
    printf("Unknown host\n");
    return 1;
  }

  addr = host->h_addr_list[0];
  printf("%s\n", inet_ntoa(*addr));

  if ((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Error creating socket\n");
    return 1;
  }

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  memcpy((char *) &saddr.sin_addr.s_addr, addr, host->h_length);
  saddr.sin_port = htons(port);

  if (connect(sockfd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
    printf("Cannot connect\n");
    return 1;
  }
  
  printf("Connect success\n");

  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  fl = fcntl(sockfd, F_GETFL, 0);
  fl |= O_NONBLOCK;
  fcntl(sockfd, F_SETFL, fl);

  pthread_t input_handler_thread;
  pthread_create(&input_handler_thread, NULL, input_handler, NULL);
  
  pthread_t network_handler_thread;
  pthread_create(&network_handler_thread, NULL, network_handler, NULL);
  
  pthread_join(network_handler_thread);
}