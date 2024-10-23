#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;

///////////////////////////////////////////////////////////////////////////////

void *clientCommunication(void *data);
void signalHandler(int sig);

///////////////////////////////////////////////////////////////////////////////

int main(void)
{
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;

   ////////////////////////////////////////////////////////////////////////////
   // SIGNAL HANDLER
   // SIGINT (Interrup: ctrl+c)
   // https://man7.org/linux/man-pages/man2/signal.2.html
   if (signal(SIGINT, signalHandler) == SIG_ERR)
   {
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // https://man7.org/linux/man-pages/man2/socket.2.html
   // https://man7.org/linux/man-pages/man7/ip.7.html
   // https://man7.org/linux/man-pages/man7/tcp.7.html
   // IPv4, TCP (connection oriented), IP (same as client)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error"); // errno set by socket()
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // SET SOCKET OPTIONS
   // https://man7.org/linux/man-pages/man2/setsockopt.2.html
   // https://man7.org/linux/man-pages/man7/socket.7.html
   // socket, level, optname, optvalue, optlen
   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEADDR,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reuseAddr");
      return EXIT_FAILURE;
   }

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(PORT);

   ////////////////////////////////////////////////////////////////////////////
   // ASSIGN AN ADDRESS WITH PORT TO SOCKET
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // ALLOW CONNECTION ESTABLISHING
   // Socket, Backlog (= count of waiting connections allowed)
   if (listen(create_socket, 5) == -1)
   {
      perror("listen error");
      return EXIT_FAILURE;
   }

   while (!abortRequested)
   {
      /////////////////////////////////////////////////////////////////////////
      // ignore errors here... because only information message
      // https://linux.die.net/man/3/printf
      printf("Waiting for connections...\n");

      /////////////////////////////////////////////////////////////////////////
      // ACCEPTS CONNECTION SETUP
      // blocking, might have an accept-error on ctrl+c
      addrlen = sizeof(struct sockaddr_in);
      if ((new_socket = accept(create_socket,
                               (struct sockaddr *)&cliaddress,
                               &addrlen)) == -1)
      {
         if (abortRequested)
         {
            perror("accept error after aborted");
         }
         else
         {
            perror("accept error");
         }
         break;
      }

      /////////////////////////////////////////////////////////////////////////
      // START CLIENT
      // ignore printf error handling
      printf("Client connected from %s:%d...\n",
             inet_ntoa(cliaddress.sin_addr),
             ntohs(cliaddress.sin_port));
      clientCommunication(&new_socket); // returnValue can be ignored
      new_socket = -1;
   }

   // frees the descriptor
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown create_socket");
      }
      if (close(create_socket) == -1)
      {
         perror("close create_socket");
      }
      create_socket = -1;
   }

   return EXIT_SUCCESS;
}
void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("\nSIGINT received, shutting down server...\n");
      abortRequested = 1;
      if (create_socket != -1)
         close(create_socket);
      if (new_socket != -1)
         close(new_socket);
   }
}

void handleSendCommand(int client_socket)
{
   char sender[9], receiver[9], subject[81], message[BUF];
   memset(sender, 0, sizeof(sender));
   memset(receiver, 0, sizeof(receiver));
   memset(subject, 0, sizeof(subject));
   memset(message, 0, sizeof(message));

   // Receive sender
   if (recv(client_socket, sender, 8, 0) <= 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Receive receiver
   if (recv(client_socket, receiver, 8, 0) <= 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Receive subject
   if (recv(client_socket, subject, 80, 0) <= 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Receive message (until ".\n" is found)
   char buffer[BUF];
   while (1)
   {
      memset(buffer, 0, BUF);
      int bytesReceived = recv(client_socket, buffer, BUF, 0);
      if (bytesReceived <= 0)
      {
         send(client_socket, "ERR\n", 4, 0);
         return;
      }
      strcat(message, buffer);
      if (strstr(buffer, "\.\n"))
         break;
   }

   // Store message in mail spool directory (e.g., "mail-spool/<receiver>")
   char directory[BUF];
   snprintf(directory, BUF, "mail-spool/%s", receiver);
   mkdir(directory, 0777);

   char filepath[BUF];
   snprintf(filepath, BUF, "%s/message.txt", directory);
   int fd = open(filepath, O_CREAT | O_WRONLY | O_APPEND, 0666);
   if (fd == -1)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   dprintf(fd, "From: %s\nTo: %s\nSubject: %s\nMessage:\n%s\n\n", sender, receiver, subject, message);
   close(fd);

   send(client_socket, "OK\n", 3, 0);
}

void handleListCommand(int client_socket)
{
   char username[9];
   memset(username, 0, sizeof(username));

   // Receive username
   if (recv(client_socket, username, 8, 0) <= 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Open user directory
   char directory[BUF];
   snprintf(directory, BUF, "mail-spool/%s", username);
   DIR *dir = opendir(directory);
   if (dir == NULL)
   {
      send(client_socket, "0\n", 3, 0);
      return;
   }

   // List messages
   struct dirent *entry;
   int messageCount = 0;
   char response[BUF] = {0};

   while ((entry = readdir(dir)) != NULL)
   {
      if (entry->d_type == DT_REG)
      {
         messageCount++;
         strcat(response, entry->d_name);
         strcat(response, "\n");
      }
   }

   closedir(dir);

   char countStr[16];
   snprintf(countStr, sizeof(countStr), "%d\n", messageCount);
   send(client_socket, countStr, strlen(countStr), 0);
   send(client_socket, response, strlen(response), 0);
}

void handleReadCommand(int client_socket)
{
   char username[9];
   int messageNumber;
   memset(username, 0, sizeof(username));

   // Receive username
   if (recv(client_socket, username, 8, 0) <= 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Receive message number
   char buffer[BUF];
   memset(buffer, 0, BUF);
   if (recv(client_socket, buffer, BUF, 0) <= 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }
   messageNumber = atoi(buffer);

   // Open user directory
   char directory[BUF];
   snprintf(directory, BUF, "mail-spool/%s", username);
   DIR *dir = opendir(directory);
   if (dir == NULL)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Find the specified message
   struct dirent *entry;
   int currentMessage = 0;
   char filepath[BUF] = {0};

   while ((entry = readdir(dir)) != NULL)
   {
      if (entry->d_type == DT_REG)
      {
         if (currentMessage == messageNumber)
         {
            snprintf(filepath, BUF, "%s/%s", directory, entry->d_name);
            break;
         }
         currentMessage++;
      }
   }

   closedir(dir);

   if (strlen(filepath) == 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Read and send the message content
   int fd = open(filepath, O_RDONLY);
   if (fd == -1)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   memset(buffer, 0, BUF);
   read(fd, buffer, BUF);
   close(fd);

   send(client_socket, "OK\n", 3, 0);
   send(client_socket, buffer, strlen(buffer), 0);
}

void handleDelCommand(int client_socket)
{
   char username[9];
   int messageNumber;
   memset(username, 0, sizeof(username));

   // Receive username
   if (recv(client_socket, username, 8, 0) <= 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Receive message number
   char buffer[BUF];
   memset(buffer, 0, BUF);
   if (recv(client_socket, buffer, BUF, 0) <= 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }
   messageNumber = atoi(buffer);

   // Open user directory
   char directory[BUF];
   snprintf(directory, BUF, "mail-spool/%s", username);
   DIR *dir = opendir(directory);
   if (dir == NULL)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Find the specified message
   struct dirent *entry;
   int currentMessage = 0;
   char filepath[BUF] = {0};

   while ((entry = readdir(dir)) != NULL)
   {
      if (entry->d_type == DT_REG)
      {
         if (currentMessage == messageNumber)
         {
            snprintf(filepath, BUF, "%s/%s", directory, entry->d_name);
            break;
         }
         currentMessage++;
      }
   }

   closedir(dir);

   if (strlen(filepath) == 0)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   // Delete the message
   if (unlink(filepath) == -1)
   {
      send(client_socket, "ERR\n", 4, 0);
      return;
   }

   send(client_socket, "OK\n", 3, 0);
}


void *clientCommunication(void *data)
{
   char buffer[BUF];
   int size;
   int *current_socket = (int *)data;

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

   do
   {
      /////////////////////////////////////////////////////////////////////////
      // RECEIVE
      size = recv(*current_socket, buffer, BUF - 1, 0);
      if (size == -1)
      {
         if (abortRequested)
         {
            perror("recv error after aborted");
         }
         else
         {
            perror("recv error");
         }
         break;
      }

      if (size == 0)
      {
         printf("Client closed remote socket\n"); // ignore error
         break;
      }

      // remove ugly debug message, because of the sent newline of client
      if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
      {
         size -= 2;
      }
      else if (buffer[size - 1] == '\n')
      {
         --size;
      }

      buffer[size] = '\0';
      printf("Message received: %s\n", buffer); // ignore error

      if (send(*current_socket, "OK", 3, 0) == -1)
      {
         perror("send answer failed");
         return NULL;
      }
   } while (strcmp(buffer, "quit") != 0 && !abortRequested);

   // closes/frees the descriptor if not already
   if (*current_socket != -1)
   {
      if (shutdown(*current_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown new_socket");
      }
      if (close(*current_socket) == -1)
      {
         perror("close new_socket");
      }
      *current_socket = -1;
   }

   return NULL;
}

void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("abort Requested... "); // ignore error
      abortRequested = 1;
      /////////////////////////////////////////////////////////////////////////
      // With shutdown() one can initiate normal TCP close sequence ignoring
      // the reference count.
      // https://beej.us/guide/bgnet/html/#close-and-shutdownget-outta-my-face
      // https://linux.die.net/man/3/shutdown
      if (new_socket != -1)
      {
         if (shutdown(new_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown new_socket");
         }
         if (close(new_socket) == -1)
         {
            perror("close new_socket");
         }
         new_socket = -1;
      }

      if (create_socket != -1)
      {
         if (shutdown(create_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown create_socket");
         }
         if (close(create_socket) == -1)
         {
            perror("close create_socket");
         }
         create_socket = -1;
      }
   }
   else
   {
      exit(sig);
   }
}
