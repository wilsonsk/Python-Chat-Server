/* 
* CS372: Project 1
* Chat Client: chatclient.c
* Skyler Wilson
* description:
	* Design and implement a simple chat system that works for a pair of users (client, server)
* requirements:
	1. chatserve starts on host A
	2. chatserve on host A waits on a port (specified on command line) for a client requrest
	3. chatclient starts on host B specifying host A's hostname and port number on command line
	4. chatclient on host B gets the user's "handle" by initial query (one word name, up to 10 chars)
		* chatclient will display this handle as a prompt on host B 
		* chatclient will prepend handle to all messages sent to host A (e.g., "SteveO> Hi!!!")
	5. chatclient on host B sends an initial message to chatserve on host A : PORTNUM
		* This causes a connection to be established between host A and host B
		* host A and host B are now peers and may alternate sending/receiving messages
		* responses from host A should have host A's "handle" prepended
	6. host A responds to host B or closes the connection with the command "quit"
	7. host B responds to host A or closes the connection with the command "quit"
	8. if the connection is not closed, repeat from 6
	9. if the connection is closed, chatserve repeats from 2 (until SIGINT is received)
	* chatserve is implemented in python
	* chatclient is implemented in c
* references:
	* see the included README.txt file for documentation of the program (chatclient.c)
*/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>	//This header file contains definitions of a number of data types used in system calls. These types are used in the next two include files
#include<sys/socket.h>	//The header file socket.h includes a number of definitions of structures needed for sockets
#include<netinet/in.h>	//The header file in.h contains constants and structures needed for internet domain addresses
#include<netdb.h>

/*chat function: 
	* inputs:
		* int socket: takes the file descriptor of a socket structure
			* this will be the socket of the server being connected to
	* outputs:
		* no program output
	* purpose:
		* within the while loop, the client and server can communicate 
			* rcvdMessage (max length of 512 bytes) is receieved from server
			* sentMessage (max length of 512 bytes) is sent to server
		* while loop (communications) end on a "quit" command from either side
*/
void error(char* msg);
void chat(int socket);

typedef enum {
	false = 0, 
	true = !false
} bool;

//rcv message size = 512 for message + 10 for 'chatserve >'
#define maxRcvdMessageSize 512
#define maxSentMessageSize 512
//buffer size of handle + sentMessage + '>'
#define maxSentFinalMessageSize 525
//handle size = 11 because 11th char == '/0' (a string terminating token)
#define	hndlLen	10

int main(int argc, char *argv[]){
	//check for 2 agrs: hostname and port number
	if(argc < 2){
		error("usage: ./chatclient <hostname> <portno>");
	}

	//connection vars
	//socket fd of server being listened on
	int sockfd;	
	int portno;
	struct sockaddr_in serv_addr;
	struct hostent* server;
	
	//creating the socket
	//arg1 = domain (AF_INET for IP); arg2 = transport protocol type (UDP = SOCK_DGRAM, TCP = SOCK_STREAM); arg3 = protocol (IP = 0)
	//check if socket was created
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket creation");
		exit(1);
	}
	
	//The gethostbyaddr() function returns a structure of type hostent for the given host address addr of length len and address type type
	//server.h_addr now contains the IP address of the hostname passed in
	server = gethostbyname(argv[1]);

	//check valid server
	if(server == NULL) {
        	fprintf(stderr,"ERROR, no such host\n");
        	exit(0);
	}


	//get port number from cmd line
	//The port number on which the server will listen for connections is passed in as an argument, and this statement uses the atoi() function to convert this from a string of digits to an integer
	portno = atoi(argv[2]);

	//initialize serv_addr

	//zero out serv_addr
	bzero((char*) &serv_addr, sizeof(serv_addr));
	
	//The variable serv_addr is a structure of type struct sockaddr_in. This structure has four fields. The first field is short sin_family, which contains a code 
	//for the address family. It should always be set to the symbolic constant AF_INET
	serv_addr.sin_family = AF_INET;

	//because the field server->h_addr is a character string, we use the function bcopy(char* s1, char* s2, int len) which copies len bytes from s1 to s2
	//this copies the IP address from server->h_addr to serv_addr struct's IP field
	bcopy((char*)server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length);

	//set port number of socket to portno arg
	//The htons(hostshort) function converts the unsigned short integer hostshort from host byte order to network byte order
	serv_addr.sin_port = htons(portno);

	//connect the socket
	//The connect function is called by the client to establish a connection to the server (remote address)
	//connect(int sockfd, struct sockaddr* address, size_t address_size): returns 0 on success, -1 on failure; size_t is an unsigned int data type
	if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
		perror("socket connection");
		exit(1);
	}
	printf("Connected to the remote host\n");	
	chat(sockfd);

	return 0;
}


void error(char* msg){
	printf("%s \n", msg);
	exit(0);
}

void chat(int socket){
	//ssize_t data type is used to represent the sizes of blocks that can be read or written in a single operation. It is similar to size_t , but must be a signed type
	ssize_t n;
	char rcvdMessage[maxRcvdMessageSize];
	char sentMessage[maxSentMessageSize];
	char sentFinalMessage[maxSentFinalMessageSize];
	bool quit = false;
	char handle[hndlLen];
	//error check var for handle size
	int check = 0;
	bool valid = false;
	char* cmdQuit;
	cmdQuit = "quit";
	//get user handle and check handle length
	while(valid != true){		
		printf("Please enter a user handle: ");
		fgets(handle, hndlLen, stdin);
		// fgets included the newline in the buffer. One of its guarantees is that the buffer will end with a newline, unless the entered line is too long for the buffer,
		// in which case it does not end with a newline, so replace newline with null terminating token
		char* pos;
		if((pos = strchr(handle, '\n')) != NULL){
			*pos = '\0';
		}
		check = strlen(handle);
		if(check > hndlLen){
			printf("error: handle too long! Max length = %d\n", hndlLen);
		}else{
			valid = true;
		}
	}

	while(quit != true){
		valid = false;
		//zero out sent message buffer
		//The bzero() function shall place n zero-valued bytes in the area pointed to by first argument
		bzero(sentMessage, maxSentMessageSize);	
	
		//get client chat message
		while(valid != true){
			printf("%s > Enter a message or the command 'quit' to end communications: ", handle);	
			fgets(sentMessage, maxSentMessageSize, stdin);	
			check = strlen(sentMessage);
			if(check > maxSentMessageSize){
				printf("error: message size too large! Max length = %d\n", maxSentMessageSize);
			}else{
				valid = true;
			}
		}
		// fgets included the newline in the buffer. One of its guarantees is that the buffer will end with a newline, unless the entered line is too long for the buffer,
		// in which case it does not end with a newline, so replace newline with null terminating token
		char* pos;
		if((pos = strchr(sentMessage, '\n')) != NULL){
			*pos = '\0';
		}
		//get new size of sentMessage after '\n' has been cut from the string
		check = strlen(sentMessage);
		
		//check if sent message is a 'quit' command: set quit = true and exit while loop
		if(strcmp(sentMessage, cmdQuit) == 0){
			printf("connection closed by client\n");
			n = write(socket, sentMessage, check);
			quit = true;	
			break;
		}	

		//store formatted string in buffer
		check = sprintf(sentFinalMessage, "%s > %s\n", handle, sentMessage);
		if(check < 0){
			error("sprintf error of sent final message");
		}
		
		//if not quit command, send/write sent final message to server
		n = write(socket, sentFinalMessage, check);
		if(n < 0){
			error("error: did not write to server");
		}
		printf("waiting for response from server...\n\n");
		//zero out received message buffer
		//The bzero() function shall place n zero-valued bytes in the area pointed to by first argument
		bzero(rcvdMessage, maxRcvdMessageSize);

		//recv() takes socket fd, pointer to buffer for storing data, max number of bytes to retrieve, config flags
		//returns number of bytes received or 0
		n = recv(socket, rcvdMessage, maxRcvdMessageSize, 0);	
		
		if(n < maxRcvdMessageSize){
			if(strcmp(rcvdMessage, cmdQuit) == 0){
				printf("connection closed by server\n");
				quit = true;
			}
			else if(n == -1){
				error("Receive message error: -1");
			}else if(0 < n < maxRcvdMessageSize){
				printf("chatserve > %s\n\n", rcvdMessage);
				
			}else if(n == 0){
				printf("end of data from server");
				quit = true;
			}
		}
	}

	close(socket);
	return;
}
