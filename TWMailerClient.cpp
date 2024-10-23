#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>

#define BUF 1024

using namespace std;

int main(int argc, char **argv) {
   if (argc < 3) {
       cerr << "Missing arguments!" << endl;
       return EXIT_FAILURE;
   }

   int port = atoi(argv[2]);
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int size;
   bool isQuit = false;

   // Create a socket for the connection
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   // Initialize the address struct and set IP/port
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_port = htons(port);
   inet_aton(argv[1], &address.sin_addr);

   // Attempt to establish a connection to the server
   if (connect(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1) {
      perror("Connect error - no server available");
      return EXIT_FAILURE;
   }

   printf("Connection with server (%s) established\n", inet_ntoa(address.sin_addr));

   // Receive welcome message from the server
   size = recv(create_socket, buffer, BUF - 1, 0);
   if (size == -1) {
      perror("recv error");
   } else if (size == 0) {
      printf("Server closed remote socket\n");
   } else {
      buffer[size] = '\0';
      printf("%s", buffer);
   }

   // Main loop to handle user input and interaction with the server
   do {
      printf(">> ");
      string input = "";
      string line = "";

      getline(cin, line);  // Get command from user

      if ("QUIT" == line) {
         isQuit = true;
         send(create_socket, "QUIT\n", 5, 0);  // Send QUIT command
         break;
      }

      // Handle SEND command
      else if ("SEND" == line) {
         send(create_socket, "SEND\n", 5, 0);  // Send SEND command

         cout << "Enter sender: ";
         getline(cin, line);
         line += "\n";
         send(create_socket, line.c_str(), line.size(), 0);  // Send sender

         cout << "Enter receiver: ";
         getline(cin, line);
         line += "\n";
         send(create_socket, line.c_str(), line.size(), 0);  // Send receiver

         cout << "Enter subject: ";
         getline(cin, line);
         line += "\n";
         send(create_socket, line.c_str(), line.size(), 0);  // Send subject

         cout << "Enter message (end with a dot '.'): " << endl;
         while (getline(cin, line)) {
            if (line == ".") {
               line = ".\n";
               send(create_socket, line.c_str(), line.size(), 0);  // End of message
               break;
            }
            line += "\n";
            send(create_socket, line.c_str(), line.size(), 0);  // Send message content
         }
      }

      // Handle LIST command
      else if ("LIST" == line) {
         send(create_socket, "LIST\n", 5, 0);  // Send LIST command

         cout << "Enter username (receiver): ";
         getline(cin, line);
         line += "\n";
         send(create_socket, line.c_str(), line.size(), 0);  // Send receiver username

         // Receive list of messages from server
         size = recv(create_socket, buffer, BUF - 1, 0);
         if (size == -1) {
            perror("recv error");
         } else if (size == 0) {
            printf("Server closed remote socket\n");
         } else {
            buffer[size] = '\0';
            printf("<< %s\n", buffer);  // Print the list of messages
         }
      }

      // Handle READ command
      else if ("READ" == line) {
         send(create_socket, "READ\n", 5, 0);  // Send READ command
         std::cout << "DEBUG: Sent READ command" << std::endl;

         // Get recipient's name
         cout << "Enter recipient's name: ";
         getline(cin, line);
         line += "\n";
         send(create_socket, line.c_str(), line.size(), 0);  // Send recipient's name
         std::cout << "DEBUG: Sent recipient: " << line << std::endl;

         // Get message subject
         cout << "Enter subject: ";
         getline(cin, line);
         line += "\n";
         send(create_socket, line.c_str(), line.size(), 0);  // Send subject
         std::cout << "DEBUG: Sent subject: " << line << std::endl;

         // Receive the message content
         size = recv(create_socket, buffer, BUF - 1, 0);
         if (size == -1) {
            perror("recv error");
         } else if (size == 0) {
            printf("Server closed remote socket\n");
         } else {
            buffer[size] = '\0';
            printf("<< %s\n", buffer);  // Print the message content
         }
      }

      // Handle DEL command
      else if ("DEL" == line) {
         send(create_socket, "DEL\n", 4, 0);  // Send DEL command

         // Get recipient's name
         cout << "Enter recipient's name: ";
         getline(cin, line);
         line += "\n";
         send(create_socket, line.c_str(), line.size(), 0);  // Send recipient's name

         // Get message subject
         cout << "Enter subject: ";
         getline(cin, line);
         line += "\n";
         send(create_socket, line.c_str(), line.size(), 0);  // Send subject

         // Receive server feedback
         size = recv(create_socket, buffer, BUF - 1, 0);
         if (size == -1) {
            perror("recv error");
         } else if (size == 0) {
            printf("Server closed remote socket\n");
         } else {
            buffer[size] = '\0';
            printf("<< %s\n", buffer);  // Print server response
         }
      }

   } while (!isQuit);

   // Close the socket when done
   if (create_socket != -1) {
      if (shutdown(create_socket, SHUT_RDWR) == -1) {
         perror("shutdown create_socket");
      }
      if (close(create_socket) == -1) {
         perror("close create_socket");
      }
      create_socket = -1;
   }

   return EXIT_SUCCESS;
}
