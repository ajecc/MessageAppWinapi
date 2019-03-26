# MessageApp - A console based messanger for Windows

This repo consists of 2 parts: the server and the client.
The connection between these 2 is made using a TCP/IP protocol. A client may connect to the server only if it is connected to the same LAN.

The server:
- Must be launched with a fixed number of maximum clients, provided when strating the app in the command line (as an argument). 
  If this limit is reached a client can't connect to the server and that client will be notified.
- The server will output the message "Success" if the binding to the port (in range 50010-50020) was successful.
- In case of any kind of initialization error, a relevant error message will be displayed.
- The server will hold the following information its clients, saved on the disk (all saved in plain text!):
    - the login creditentials of any user
    - message history
    - pending messages - the only ones that can get cleared
- The server handles the input of the clients concurrently, as thread-safe as possible and as "network-safe" as possible.

The client:
- Will connect to the server
- After connecting successfully, a client may communicate with any other client through the server
- Client can call any of the following:
    - echo <message>
          - this will also print the message on the server
          - can be used with no user logged in
    - register <username> <password>
          - username can only contain alphanumeric character
          - the password has to be at least 5 characters long, contain an uppercase letter and a non alphanumeric character
    - login <username> <password>
    - logout
    - msg <user> <message>
    - broadcast <message>
          - the sender will not see this message
    - sendfile <user> <filepath>
        - sends a file to a user and it is beeing saved in the directory where the receiver user's client executable is
        - if a file with the same name exists in that directory, the 2 files will be concatenated
        - if any of the users logs out, the sending will stop
    - list
        - lists every user logged in
        - can be used with no user logged in
    - history <username> <count>
        - outputs the history between the current user and the one identified by <username> up to <count> messages.
     - exit
        - exits the application, logging out the user
 - The client may not send messages longer than 255 characters
 - In case of error, a relevant message is displayed

Most of the code in MessageCommunicationLib is not written by me. It was handed to me by one of my teachers for this project. However, communication_string_funciton.h is my own work and serves the purpose of providing some functions used by both the server and the client for transforming the data buffer in a generic string and working with these strings.
