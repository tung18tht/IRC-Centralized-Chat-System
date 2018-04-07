#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>
#include <stdlib.h>

#define MAX_CLIENT 100
#define BUFFER_SIZE 1024

int sockfd, fl, clen, clientfd, count_client = 0;
int child_to_parent_pipe[MAX_CLIENT][2] = {{0}};
int parent_to_child_pipe[MAX_CLIENT][2] = {{0}};
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

char* get_help_message() {
  return "\n"
  "*****************************************************\n"
  "*        Welcome to command-line IRC Program        *\n"
  "*                                                   *\n"
  "*    You are connected to the server, just type     *\n"
  "*               your message to chat                *\n"
  "*                                                   *\n"
  "* Following are some useful commands:               *\n"
  "*                                                   *\n"
  "*   /help               Show this message           *\n"
  "*   /id                 Get your client id          *\n"
  "*   /list               Show all connected clients  *\n"
  "*   /pm [id] [message]  Send PM to a client         *\n"
  "*   /quit               Exit the program            *\n"
  "*                                                   *\n"
  "*****************************************************\n";
}

void get_list_message(int called_client, char *message) {
  sprintf(message, "[Server] Following is the list of connected clients:");
  for (int i=0; i<MAX_CLIENT; i++) {
    if (child_to_parent_pipe[i][0] > 0) {
      if (called_client == i) {
        sprintf(message, "%s\n       * Client %d  <-- YOU", message, i);
      } else {
        sprintf(message, "%s\n       * Client %d", message, i);
      }
    }
  }
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
      
      if(count_client == MAX_CLIENT) {
        disconnect(clientfd);
      } else {  
        int i;
        for (i = 0; i < MAX_CLIENT; i++) {
          if ((parent_to_child_pipe[i][0] == 0) && (parent_to_child_pipe[i][1] == 0) && (child_to_parent_pipe[i][0] == 0) && (child_to_parent_pipe[i][1] == 0)){
            pipe (parent_to_child_pipe[i]);
            pipe (child_to_parent_pipe[i]);
            count_client++;
            printf("Client %d connected (currently %d clients connected)\n", i, count_client);
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
            
            char message[BUFFER_SIZE];
            strcpy(message, get_help_message());
            write(clientfd, message, sizeof(message));

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
    }
    
    for (int i=0; i<MAX_CLIENT; i++) {
      if (child_to_parent_pipe[i][0] > 0 && FD_ISSET(child_to_parent_pipe[i][0], &set)) {
        char message[BUFFER_SIZE];
        if(read(child_to_parent_pipe[i][0], message, sizeof(message)) < 0){
          return 1;
        }
        printf("Message from Client %d: %s\n", i, message);
        
        if(strcmp(message, "/quit") == 0) {
          clean_pipe(i);
          count_client--;
          printf("Client %d disconnected (currently %d clients connected)\n", i, count_client);
        } else if (strcmp(message, "/id") == 0) {
          sprintf(message, "[Server] Your ID is: %d", i);
          write(parent_to_child_pipe[i][1], message, sizeof(message));
        } else if (strcmp(message, "/help") == 0) {
          strcpy(message, get_help_message());
          write(parent_to_child_pipe[i][1], message, sizeof(message));
        } else if (strcmp(message, "/list") == 0) {
          get_list_message(i, message);
          write(parent_to_child_pipe[i][1], message, sizeof(message));
        } else if (strcmp(strtok(message, " "), "/pm") == 0) {
          char *content, msg_with_header[BUFFER_SIZE];
          int dest_id = atoi(strtok(NULL, " "));
          sprintf(msg_with_header, "Client %d [PM]: ", dest_id);
          content = strtok(NULL, "");
          strcat(msg_with_header, content);
          write(parent_to_child_pipe[dest_id][1], msg_with_header, sizeof(msg_with_header));
        } else {
          char msg_with_header[BUFFER_SIZE];
          sprintf(msg_with_header, "Client %d: ", i);
          strcat(msg_with_header, message);
          for (int j=0; j<MAX_CLIENT; j++) {
            if ((parent_to_child_pipe[j][1] > 0) && (j != i)) {
              write(parent_to_child_pipe[j][1], msg_with_header, sizeof(msg_with_header));
            }
          }
        }
      }
    }
  }
}