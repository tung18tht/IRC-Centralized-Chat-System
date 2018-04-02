#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv) {
  struct sockaddr_in saddr;
  struct in_addr *addr;
  struct hostent *host;
  int sockfd, fl;
  unsigned short port = 8784;
  char message[256];

  if(argc == 2) {
    host = gethostbyname(argv[1]);
  } else {
    printf("Enter your domain: ");
    char domain[100];
    scanf("%s", domain);
    host = gethostbyname(domain);
  }

  if (host == NULL) {
    printf("Unknow host\n");
  } else {
    addr = host->h_addr_list[0];
    printf("%s\n", inet_ntoa(*addr));
  }

  if ((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Error creating socket\n");
  }

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  memcpy((char *) &saddr.sin_addr.s_addr, addr, host->h_length);
  saddr.sin_port = htons(port);

  if (connect(sockfd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
    printf("Cannot connect\n");
  } else {
    printf("Connect success\n");
  }

  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  fl = fcntl(sockfd, F_GETFL, 0);
  fl |= O_NONBLOCK;
  fcntl(sockfd, F_SETFL, fl);

  while(1) {
    printf("Client: ");
    scanf("%s", message);
    write(sockfd, message, sizeof(message));

    if(read(sockfd, message, sizeof(message)) > 0) {
      printf("Server: %s\n", message);
    }
  }
}