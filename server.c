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

unsigned int clen;
int sockfd, fl, clientfd, count_client = 0;
int child_to_parent_pipe[MAX_CLIENT][2] = {{0}};
int parent_to_child_pipe[MAX_CLIENT][2] = {{0}};
int clientfds[MAX_CLIENT] = {0};
struct sockaddr_in saddr, caddr;
unsigned short port = 8784;

void disconnect(int socket) {
  shutdown(socket, SHUT_RDWR);
  close(socket);
}

void clean_pipe_and_fd(int client) {
  close(parent_to_child_pipe[client][1]);
  close(child_to_parent_pipe[client][0]);
  child_to_parent_pipe[client][0] = 0;
  child_to_parent_pipe[client][1] = 0;
  parent_to_child_pipe[client][0] = 0;
  parent_to_child_pipe[client][1] = 0;
  clientfds[client] = 0;
}

char* get_server_help_message() {
  return
  "Here are list of commands to manage the server:\n"
  "   help                  Show this message\n"
  "   list                  Show all connected clients\n"
  "   pm [id] [message]     Send PM to a client\n"
  "   broadcast [message]   Broadcast message to all connected clients\n"
  "   dc [id]               Disconnect a client\n"
  "   shutdown              Close all connections and shutdown server"
  ;
}

char* get_help_message() {
  return "\n"
  "*****************************************************\n"
  "*        Welcome to command-line IRC Program        *\n"
  "*---------------------------------------------------*\n"
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
  "*****************************************************\n"
  ;
}

void get_list_message(int called_client, char *message) {
  if (count_client != 0) {
    sprintf(message, "[Server] Following is the list of connected clients:");
    for (int i=0; i<MAX_CLIENT; i++) {
      if (clientfds[i] > 0) {
        if (called_client == i) {
          sprintf(message, "%s\n       * Client %d  <-- YOU", message, i);
        } else {
          sprintf(message, "%s\n       * Client %d", message, i);
        }
      }
    }
  } else {
    sprintf(message, "[Server] Currently there are no connected clients.");
  }
}

void broadcast(char *message) {
  char msg_to_client[BUFFER_SIZE];
  sprintf(msg_to_client, "[Server] ");
  strcat(msg_to_client, message);
  for (int i = 0; i < MAX_CLIENT; i++) {
    if (clientfds[i] > 0) {
      write(parent_to_child_pipe[i][1], msg_to_client, sizeof(msg_to_client));
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

  printf("Server created, listening for connections...\n%s\n> ", get_server_help_message());
  fflush(stdout);

  while(1) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(0, &set);
    FD_SET(sockfd, &set);
    
    int maxfd = sockfd;
    for(int i=0; i<MAX_CLIENT; i++) {
      if (clientfds[i] > 0) {
        FD_SET(child_to_parent_pipe[i][0], &set);
        if (child_to_parent_pipe[i][0] > maxfd) {
          maxfd = child_to_parent_pipe[i][0];
        }
      }
    }
    
    select(maxfd+1, &set, NULL, NULL, NULL);

    if (FD_ISSET(0, &set)) {
      char command[BUFFER_SIZE];
      fgets(command, sizeof(command), stdin);
      if (strlen(command) == 1) {
        printf("> ");
        fflush(stdout);
      } else {
        if ((strlen(command) > 0) && (command[strlen (command) - 1] == '\n')) {
          command[strlen (command) - 1] = '\0';
        }
        char cmd_copy[BUFFER_SIZE];
        strcpy(cmd_copy, command);
        char *first_token = strtok(cmd_copy, " ");
        if (strcmp(first_token, "help") == 0) {
          printf("%s\n> ", get_server_help_message());
          fflush(stdout);
        } else if (strcmp(first_token, "list") == 0) {
          char message[BUFFER_SIZE];
          get_list_message(-1, message);
          printf("%s\n> ", message);
          fflush(stdout);
        } else if (strcmp(first_token, "pm") == 0) {
          char *check, *dest_id_str;
          dest_id_str = strtok(NULL, " ");
          if (dest_id_str != NULL) {
            int dest_id = strtol(dest_id_str, &check, 10);
            if ((*check == '\0') && (clientfds[dest_id] > 0)) {
              char *content, msg_to_client[BUFFER_SIZE];
              content = strtok(NULL, "");
              if (content != NULL) {
                sprintf(msg_to_client, "[Server] ");
                strcat(msg_to_client, content);
                write(parent_to_child_pipe[dest_id][1], msg_to_client, sizeof(msg_to_client));
              }
            } else {
              printf("Message didn't send. Cannot find Client %s\n", dest_id_str);
            }
          }
          printf("> ");
          fflush(stdout);
        } else if (strcmp(first_token, "broadcast") == 0) {
          char *message;
          message = strtok(NULL, "");
          if (message != NULL) {
            broadcast(message);
          }
          printf("> ");
          fflush(stdout);
        } else if (strcmp(first_token, "dc") == 0) {
          char *check, *client_id_str;
          client_id_str = strtok(NULL, " ");
          if (client_id_str != NULL) {
            int client_id = strtol(client_id_str, &check, 10);
            if ((*check == '\0') && (clientfds[client_id] > 0)) {
              disconnect(clientfds[client_id]);
              clean_pipe_and_fd(client_id);
              count_client--;
              printf("Kicked Client %d (currently %d clients connected)\n", client_id, count_client);
            } else {
              printf("Cannot find Client %s\n", client_id_str);
            }
          }
          printf("> ");
          fflush(stdout);
        } else if (strcmp(first_token, "shutdown") == 0) {
          broadcast("Server is shutting down in 3...2...1...");
          sleep(3);
          for (int i = 0; i < MAX_CLIENT; i++) {
            if (clientfds[i] > 0) {
              disconnect(clientfds[i]);
              clean_pipe_and_fd(i);
            }
          }
          printf("All connections closed, shutting down server...\n");
          return 0;
        } else {
          printf("Unknown command. Type 'help' for the manual.\n> ");
          fflush(stdout);
        }
      }
    }

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
          if (clientfds[i] == 0){
            clientfds[i] = clientfd;
            pipe (parent_to_child_pipe[i]);
            pipe (child_to_parent_pipe[i]);
            count_client++;
            printf("\nClient %d connected (currently %d clients connected)\n> ", i, count_client);
            fflush(stdout);
            break;
          }
        }
        
        switch(fork()) {
          case -1:
            printf("\nCannot create process!\n");
            return 1;
          case 0:
            close(parent_to_child_pipe[i][1]);
            close(child_to_parent_pipe[i][0]);
            
            char message[BUFFER_SIZE];
            strcpy(message, get_help_message());
            write(clientfds[i], message, sizeof(message));

            while(1) {
              fd_set set;
              FD_ZERO(&set);
              FD_SET(parent_to_child_pipe[i][0], &set);
              FD_SET(clientfds[i], &set);
              
              int maxfd = clientfds[i] > parent_to_child_pipe[i][0] ? clientfds[i] : parent_to_child_pipe[i][0];
              
              select(maxfd + 1, &set, NULL, NULL, NULL);
              
              if (FD_ISSET(parent_to_child_pipe[i][0], &set)) {
                char message[BUFFER_SIZE];
                if(read(parent_to_child_pipe[i][0], message, sizeof(message)) > 0) {
                  write(clientfds[i], message, sizeof(message));
                } else {
                  return 1;
                }
              }
              
              if(FD_ISSET(clientfds[i], &set)){
                char message[BUFFER_SIZE];
                if(read(clientfds[i], message, sizeof(message)) > 0) {
                  write(child_to_parent_pipe[i][1], message, sizeof(message));
                  if(strcmp(message, "/quit") == 0) {
                    close(parent_to_child_pipe[i][0]);
                    close(child_to_parent_pipe[i][1]);
                    return 0;
                  }
                } else {
                  sprintf(message, "/quit");
                  write(child_to_parent_pipe[i][1], message, sizeof(message));
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
      if (clientfds[i] > 0 && FD_ISSET(child_to_parent_pipe[i][0], &set)) {
        char message[BUFFER_SIZE];
        if(read(child_to_parent_pipe[i][0], message, sizeof(message)) < 0){
          return 1;
        }
        printf("\nMessage from Client %d: %s\n> ", i, message);
        fflush(stdout);

        char msg_copy[BUFFER_SIZE];
        strcpy(msg_copy, message);
        char *first_token = strtok(msg_copy, " ");
        if (strcmp(first_token, "/pm") == 0) {
          char *check, *dest_id_str;
          dest_id_str = strtok(NULL, " ");
          if (dest_id_str != NULL) {
            int dest_id = strtol(dest_id_str, &check, 10);
            if ((*check == '\0') && (clientfds[dest_id] > 0)) {
              char *content, msg_to_client[BUFFER_SIZE];
              content = strtok(NULL, "");
              if (content != NULL) {
                sprintf(msg_to_client, "Client %d [PM]: ", i);
                strcat(msg_to_client, content);
                write(parent_to_child_pipe[dest_id][1], msg_to_client, sizeof(msg_to_client));
              }
            } else {
              char msg_to_client[BUFFER_SIZE];
              sprintf(msg_to_client, "[Server] Message didn't send. Cannot find Client ");
              strcat(msg_to_client, dest_id_str);
              write(parent_to_child_pipe[i][1], msg_to_client, sizeof(msg_to_client));
            }
          }
        } else if(strcmp(first_token, "/quit") == 0) {
          disconnect(clientfds[i]);
          clean_pipe_and_fd(i);
          count_client--;
          printf("\nClient %d disconnected (currently %d clients connected)\n> ", i, count_client);
          fflush(stdout);
        } else if (strcmp(first_token, "/id") == 0) {
          char msg_to_client[BUFFER_SIZE];
          sprintf(msg_to_client, "[Server] Your ID is: %d", i);
          write(parent_to_child_pipe[i][1], msg_to_client, sizeof(msg_to_client));
        } else if (strcmp(first_token, "/help") == 0) {
          char msg_to_client[BUFFER_SIZE];
          strcpy(msg_to_client, get_help_message());
          write(parent_to_child_pipe[i][1], msg_to_client, sizeof(msg_to_client));
        } else if (strcmp(first_token, "/list") == 0) {
          char msg_to_client[BUFFER_SIZE];
          get_list_message(i, msg_to_client);
          write(parent_to_child_pipe[i][1], msg_to_client, sizeof(msg_to_client));
        } else {
          char msg_to_client[BUFFER_SIZE];
          sprintf(msg_to_client, "Client %d: ", i);
          strcat(msg_to_client, message);
          for (int j=0; j<MAX_CLIENT; j++) {
            if ((clientfds[j] > 0) && (j != i)) {
              write(parent_to_child_pipe[j][1], msg_to_client, sizeof(msg_to_client));
            }
          }
        }
      }
    }
  }
}