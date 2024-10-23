#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUF 1024

void sendCommand(int socket, const char *command);
void handleSend(int socket);
void handleList(int socket);
void handleRead(int socket);
void handleDel(int socket);

int main(int argc, char **argv)
{
   if (argc != 3)
   {
      printf("Usage: %s <ip> <port>\n", argv[0]);
      return EXIT_FAILURE;
   }

   int create_socket;
   struct sockaddr_in address;
   char buffer[BUF];

   // Create socket
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   // Set server address
   address.sin_family = AF_INET;
   address.sin_port = htons(atoi(argv[2]));
   if (inet_pton(AF_INET, argv[1], &address.sin_addr) <= 0)
   {
      perror("Address conversion error");
      return EXIT_FAILURE;
   }

   // Connect to server
   if (connect(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("Connect error");
      return EXIT_FAILURE;
   }

   int running = 1;
   while (running)
   {
      printf("Choose a command: SEND, LIST, READ, DEL, QUIT\n");
      fgets(buffer, BUF, stdin);
      buffer[strcspn(buffer, "\n")] = 0;

      if (strcmp(buffer, "SEND") == 0)
      {
         handleSend(create_socket);
      }
      else if (strcmp(buffer, "LIST") == 0)
      {
         handleList(create_socket);
      }
      else if (strcmp(buffer, "READ") == 0)
      {
         handleRead(create_socket);
      }
      else if (strcmp(buffer, "DEL") == 0)
      {
         handleDel(create_socket);
      }
      else if (strcmp(buffer, "QUIT") == 0)
      {
         sendCommand(create_socket, "QUIT\n");
         running = 0;
      }
      else
      {
         printf("Unknown command. Please try again.\n");
      }
   }

   close(create_socket);
   return EXIT_SUCCESS;
}

void sendCommand(int socket, const char *command)
{
   if (send(socket, command, strlen(command), 0) == -1)
   {
      perror("Send error");
   }
}

void handleSend(int socket)
{
   char sender[9], receiver[9], subject[81], message[BUF];

   printf("Enter sender (max 8 chars): ");
   fgets(sender, sizeof(sender), stdin);
   sender[strcspn(sender, "\n")] = 0;

   printf("Enter receiver (max 8 chars): ");
   fgets(receiver, sizeof(receiver), stdin);
   receiver[strcspn(receiver, "\n")] = 0;

   printf("Enter subject (max 80 chars): ");
   fgets(subject, sizeof(subject), stdin);
   subject[strcspn(subject, "\n")] = 0;

   printf("Enter message (end with a single dot on a line):\n");
   memset(message, 0, BUF);
   char line[BUF];
   while (fgets(line, BUF, stdin))
   {
      if (strcmp(line, ".\n") == 0)
      {
         break;
      }
      strcat(message, line);
   }

   char command[BUF];
   snprintf(command, BUF, "SEND\n%s\n%s\n%s\n%s.\n", sender, receiver, subject, message);
   sendCommand(socket, command);

   char response[BUF];
   memset(response, 0, BUF);
   if (recv(socket, response, BUF, 0) > 0)
   {
      printf("Server response: %s", response);
   }
}

void handleList(int socket)
{
   char username[9];

   printf("Enter username (max 8 chars): ");
   fgets(username, sizeof(username), stdin);
   username[strcspn(username, "\n")] = 0;

   char command[BUF];
   snprintf(command, BUF, "LIST\n%s\n", username);
   sendCommand(socket, command);

   char response[BUF];
   memset(response, 0, BUF);
   if (recv(socket, response, BUF, 0) > 0)
   {
      printf("Server response: %s", response);
   }
}

void handleRead(int socket)
{
   char username[9];
   char messageNumber[BUF];

   printf("Enter username (max 8 chars): ");
   fgets(username, sizeof(username), stdin);
   username[strcspn(username, "\n")] = 0;

   printf("Enter message number: ");
   fgets(messageNumber, sizeof(messageNumber), stdin);
   messageNumber[strcspn(messageNumber, "\n")] = 0;

   char command[BUF];
   snprintf(command, BUF, "READ\n%s\n%s\n", username, messageNumber);
   sendCommand(socket, command);

   char response[BUF];
   memset(response, 0, BUF);
   if (recv(socket, response, BUF, 0) > 0)
   {
      printf("Server response: %s", response);
   }
}

void handleDel(int socket)
{
   char username[9];
   char messageNumber[BUF];

   printf("Enter username (max 8 chars): ");
   fgets(username, sizeof(username), stdin);
   username[strcspn(username, "\n")] = 0;

   printf("Enter message number: ");
   fgets(messageNumber, sizeof(messageNumber), stdin);
   messageNumber[strcspn(messageNumber, "\n")] = 0;

   char command[BUF];
   snprintf(command, BUF, "DEL\n%s\n%s\n", username, messageNumber);
   sendCommand(socket, command);

   char response[BUF];
   memset(response, 0, BUF);
   if (recv(socket, response, BUF, 0) > 0)
   {
      printf("Server response: %s", response);
   }
}