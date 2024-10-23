#include <csignal>
#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <sstream>

#define BUF 1024
#define PORT 6543
#define MAX_PATH 512

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;
std::string dirname;

void handleSendCommand(int socket);
void handleListCommand(int socket);
void handleReadCommand(int socket);
void handleDelCommand(int socket);
void clientCommunication(int socket);
void signalHandler(int sig);

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Missing arguments!" << std::endl;
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    dirname = argv[2];

    socklen_t addrlen;
    struct sockaddr_in address, cliaddress;
    int reuseValue = 1;

    // Signal handling for graceful shutdown
    signal(SIGINT, signalHandler);

    // Create server socket
    create_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (create_socket == -1) {
        std::cerr << "Socket error" << std::endl;
        return EXIT_FAILURE;
    }

    setsockopt(create_socket, SOL_SOCKET, SO_REUSEADDR, &reuseValue, sizeof(reuseValue));

    // Initialize server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket to the address
    if (bind(create_socket, (struct sockaddr*)&address, sizeof(address)) == -1) {
        std::cerr << "Bind error" << std::endl;
        return EXIT_FAILURE;
    }

    // Listen for incoming connections
    if (listen(create_socket, 5) == -1) {
        std::cerr << "Listen error" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Waiting for connections..." << std::endl;

    while (!abortRequested) {
        addrlen = sizeof(struct sockaddr_in);
        new_socket = accept(create_socket, (struct sockaddr*)&cliaddress, &addrlen);
        if (new_socket == -1) {
            if (abortRequested) {
                std::cerr << "Accept error after abort" << std::endl;
            }
            break;
        }

        std::cout << "Client connected from " << inet_ntoa(cliaddress.sin_addr) << ":" << ntohs(cliaddress.sin_port) << std::endl;
        clientCommunication(new_socket);
        new_socket = -1;
    }

    close(create_socket);
    return EXIT_SUCCESS;
}

void clientCommunication(int socket) {
    char buffer[BUF];
    int size;

    // Send welcome message to the client
    std::string welcomeMessage = "Welcome to TWMailer!\nPlease enter one of the following commands:\n--> SEND\n--> LIST\n--> READ\n--> DEL\n--> QUIT\n";
    send(socket, welcomeMessage.c_str(), welcomeMessage.size(), 0);

    while (true) {
        size = recv(socket, buffer, BUF - 1, 0);
        if (size <= 0) break;

        buffer[size] = '\0';
        std::string command(buffer);

        // Handle the different commands from the client
        if (command.find("SEND") == 0) {
            handleSendCommand(socket);
        } else if (command.find("LIST") == 0) {
            handleListCommand(socket);
        } else if (command.find("READ") == 0) {
            handleReadCommand(socket);
        } else if (command.find("DEL") == 0) {
            handleDelCommand(socket);
        } else if (command.find("QUIT") == 0) {
            break;
        } else {
            std::cout << "Unknown command received: " << command << std::endl;
        }
    }

    close(socket);
}

void handleSendCommand(int socket) {
    char buffer[BUF];
    std::string sender, receiver, subject, messageContent;
    int size;

    // Receive and process sender, receiver, and subject information
    size = recv(socket, buffer, BUF - 1, 0);
    if (size <= 0) return;
    buffer[size] = '\0';
    sender = buffer;
    sender.pop_back();

    size = recv(socket, buffer, BUF - 1, 0);
    if (size <= 0) return;
    buffer[size] = '\0';
    receiver = buffer;
    receiver.pop_back();

    size = recv(socket, buffer, BUF - 1, 0);
    if (size <= 0) return;
    buffer[size] = '\0';
    subject = buffer;
    subject.pop_back();

    // Receive message content until a dot (.) is found
    while (true) {
        size = recv(socket, buffer, BUF - 1, 0);
        if (size <= 0) break;
        buffer[size] = '\0';
        if (strcmp(buffer, ".\n") == 0 || strcmp(buffer, ".\r\n") == 0 || strcmp(buffer, ".") == 0) break;
        messageContent += buffer;
    }

    // Save the message to the receiver's directory
    std::string directory = "./mail-spool-directory/" + receiver;
    mkdir(directory.c_str(), 0777);
    std::string filename = directory + "/message_" + std::to_string(time(NULL)) + ".txt";
    FILE* file = fopen(filename.c_str(), "w");
    if (file) {
        fprintf(file, "From: %s\nSubject: %s\n%s\n", sender.c_str(), subject.c_str(), messageContent.c_str());
        fclose(file);
        send(socket, "OK\n", 3, 0);
    } else {
        send(socket, "ERR\n", 4, 0);
    }
}

void handleListCommand(int socket) {
    char buffer[BUF];
    std::string receiver;
    int size;

    std::cout << "DEBUG: Received LIST command" << std::endl;

    // Receive the username (receiver)
    size = recv(socket, buffer, BUF - 1, 0);
    if (size <= 0) {
        std::cerr << "DEBUG: Error receiving username for LIST." << std::endl;
        return;
    }
    buffer[size] = '\0';
    receiver = buffer;
    receiver.pop_back();  // Remove newline

    std::cout << "DEBUG: Received username for LIST: " << receiver << std::endl;

    // Ensure that the command checks the receiver's directory for messages
    std::string directory = "./mail-spool-directory/" + receiver;
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        send(socket, "No messages found.\n", 19, 0);  // If directory does not exist, there are no messages
        return;
    }

    struct dirent* entry;
    std::string response;
    int count = 0;

    // Read messages from the receiver's directory
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "message_")) {
            // Read the subject from the message file
            std::string filepath = directory + "/" + entry->d_name;
            FILE* file = fopen(filepath.c_str(), "r");
            if (file) {
                char line[BUF];

                // First line is "From: ..."
                fgets(line, sizeof(line), file);
                std::string from = std::string(line).substr(6);  // Skip "From: "

                // Second line is "Subject: ..."
                fgets(line, sizeof(line), file);
                std::string subject = std::string(line).substr(9);  // Skip "Subject: "

                fclose(file);

                // Append the message info to the response
                response += "From: " + from + "Subject: " + subject + "\n";
                count++;
            }
        }
    }

    closedir(dir);

    if (count == 0) {
        response = "No messages found.\n";
    } else {
        response = std::to_string(count) + " message(s) found:\n" + response;
    }

    // Send the list of messages to the client
    send(socket, response.c_str(), response.size(), 0);
}

void handleReadCommand(int socket) {
    char buffer[BUF];
    std::string recipient, subject;
    int size;

    // Receive the recipient and subject
    size = recv(socket, buffer, BUF - 1, 0);
    if (size <= 0) return;
    buffer[size] = '\0';
    recipient = buffer;
    recipient.pop_back();

    size = recv(socket, buffer, BUF - 1, 0);
    if (size <= 0) return;
    buffer[size] = '\0';
    subject = buffer;
    subject.pop_back();

    // Find and send the message content
    std::string directory = "./mail-spool-directory/" + recipient;
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        send(socket, "Recipient not found.\n", 21, 0);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "message_")) {
            std::string filepath = directory + "/" + entry->d_name;
            FILE* file = fopen(filepath.c_str(), "r");
            if (file) {
                char file_subject[81];
                fgets(file_subject, sizeof(file_subject), file);
                fgets(file_subject, sizeof(file_subject), file);
                if (subject == std::string(file_subject + 9).substr(0, subject.size())) {
                    std::string response, line;
                    while (fgets(buffer, sizeof(buffer), file)) {
                        response += buffer;
                    }
                    fclose(file);
                    send(socket, response.c_str(), response.size(), 0);
                    closedir(dir);
                    return;
                }
                fclose(file);
            }
        }
    }
    closedir(dir);
    send(socket, "Message not found.\n", 19, 0);
}

void handleDelCommand(int socket) {
    char buffer[BUF];
    std::string recipient, subject;
    int size;

    // Receive the recipient and subject
    size = recv(socket, buffer, BUF - 1, 0);
    if (size <= 0) return;
    buffer[size] = '\0';
    recipient = buffer;
    recipient.pop_back();

    size = recv(socket, buffer, BUF - 1, 0);
    if (size <= 0) return;
    buffer[size] = '\0';
    subject = buffer;
    subject.pop_back();

    // Delete the message
    std::string directory = "./mail-spool-directory/" + recipient;
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        send(socket, "Recipient not found.\n", 21, 0);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "message_")) {
            std::string filepath = directory + "/" + entry->d_name;
            FILE* file = fopen(filepath.c_str(), "r");
            if (file) {
                char file_subject[81];
                fgets(file_subject, sizeof(file_subject), file);
                fgets(file_subject, sizeof(file_subject), file);
                if (subject == std::string(file_subject + 9).substr(0, subject.size())) {
                    fclose(file);
                    if (remove(filepath.c_str()) == 0) {
                        send(socket, "Message deleted.\n", 18, 0);
                    } else {
                        send(socket, "Error deleting message.\n", 25, 0);
                    }
                    closedir(dir);
                    return;
                }
                fclose(file);
            }
        }
    }
    closedir(dir);
    send(socket, "Message not found.\n", 19, 0);
}

void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "Server shutdown requested..." << std::endl;
        abortRequested = 1;

        // Close any open sockets
        if (new_socket != -1) {
            shutdown(new_socket, SHUT_RDWR);
            close(new_socket);
        }

        if (create_socket != -1) {
            shutdown(create_socket, SHUT_RDWR);
            close(create_socket);
        }
    }
}