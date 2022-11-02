# Real_Time_Chat_Application

## Features 

The client can send messages to the server (which sends them back to the appropriate clients). A Client can also send
commands to the server, in the form: serv-request[{command} {args}].

### List of the different commands available 

* serv-request[co_list]: gives the list of people connected to the server.
* serv-request[create_mp_disc {name} {user2} {pass}]: to create a private discussion between the person executing the command and the user2 specified in the arguments. {name} is the name of the discussion and {pass} its password. 
* serv-request[create_group_disc {private} {name} {pass} {user1} ... {userN}]: to create a group chat that can be private (if {private} = 1) or public (if {private} = 0). If the discussion is public, no need to specify usernames. 
Tthe chat creator is automatically included as a user. Pass is the password for the discussion.
* serv-request[disc_list]: gives the list of discussions available on the server. The discussions are separated according to whether they are group chats or not. In group chats, public chats are displayed in green and private ones are displayed in red.
* serv-request[change_disc {name} {pass}]: to join another discussion group. We first test the existence of the
discussion and the customer's right to access this discussion (the customer must be one of the members and enter the correct password). Pass is the password to provide to enter in the discussion if the latter requires one (private discussion or discussion of private group).
* serv-request[get_id_disc {nom}] : gives the id associated with a certain discussion name.

The messages of a discussion are saved into a file and displayed when a person connects to the discussion. If there are too many messages in the discussion, the latest messages are displayed.  
By default, all clients are on the Landing chat, which is a public group chat created automatically by the server.

## Compilation

To compile the program, simply go to the Makefile directory and run the make all command.

## Run

To run the server program, simply enter the command: ./Serveur/server.
To run the client program, simply enter the command: ./Client/client {IP} {UserName} , where IP is the address to which we want to connect (server address and therefore probably the loopback address). 
