### TW-Mailer Protocol Specification
#### by Niklas Brandtner and Sonja Lechner

This document outlines the protocol needed for the **TW-Mailer** project, where a socket-based client-server application is developed in C/C++ to send and receive messages similar to an internal mail-server.

#### 1. **General Format**
All communication between the client and server is conducted via plain-text commands, with lines separated by new-line (`\n`) characters. Each command or message is terminated with either a new-line (`\n`) or a dot followed by a new-line (`.\n`), depending on the command type.

#### 2. **Client Usage**
```bash
./twmailer-client <ip> <port>
```
- `<ip>`: The IP address of the server.
- `<port>`: The server’s listening port.

#### 3. **Server Usage**
```bash
./twmailer-server <port> <mail-spool-directory>
```
- `<port>`: The port number for listening to client connections.
- `<mail-spool-directory>`: The directory where all incoming messages will be stored.

#### 4. **Supported Commands**

##### **SEND**
The client sends a message to the server. The message is saved in the recipient’s directory on the server.

###### Command Format:
```plaintext
SEND\n
<Sender>\n
<Receiver>\n
<Subject>\n
<Message (multi-line; terminated with a dot)>\n
.\n
```
- `<Sender>`: The username of the sender (max 8 characters).
- `<Receiver>`: The username of the recipient (max 8 characters).
- `<Subject>`: The subject of the message (max 80 characters).
- `<Message>`: The multi-line content of the message, terminated with a dot (`.`) on a new line.

###### Server Response:
- `OK\n`: If the message was successfully received and stored.
- `ERR\n`: If there was an error.

##### **LIST**
The client requests a list of all messages received by a specific user.

###### Command Format:
```plaintext
LIST\n
<Username>\n
```
- `<Username>`: The username for whom to list the messages.

###### Server Response:
- `<count>\n`: Number of messages found for the user.
- `<Subject 1>\n<Subject 2>\n...\n<Subject N>\n`: List of subjects for all messages, one per line.

If the user has no messages or does not exist, the server responds with:
```plaintext
0\n
```

##### **READ**
The client requests to read a specific message for a user, based on its message number.

###### Command Format:
```plaintext
READ\n
<Username>\n
<Message-Number>\n
```
- `<Username>`: The recipient username.
- `<Message-Number>`: The index number of the message to be read.

###### Server Response:
- `OK\n<Complete message content>\n`: If the message is found.
- `ERR\n`: If the message number does not exist or an error occurred.

##### **DEL**
The client requests to delete a specific message for a user, based on its message number.

###### Command Format:
```plaintext
DEL\n
<Username>\n
<Message-Number>\n
```
- `<Username>`: The recipient username.
- `<Message-Number>`: The index number of the message to be deleted.

###### Server Response:
- `OK\n`: If the message was successfully deleted.
- `ERR\n`: If the message number does not exist or an error occurred.

##### **QUIT**
The client disconnects from the server.

###### Command Format:
```plaintext
QUIT\n
```
- The server does not respond to the `QUIT` command, it simply closes the connection.

#### 5. **Additional Notes**
- Messages are stored in a directory corresponding to the recipient's username.
- Each message is stored in a file, and the file contains details about the sender, subject, and message body.
- The `mail-spool-directory` provided at server startup will contain subdirectories for each user, where individual messages will be stored as files.
