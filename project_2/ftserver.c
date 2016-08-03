/*
* CS372: Project 2
* FT Server: ftserve.c
* Skyler Wilson
* description:
	* Design and implement a simple FTP service between a server and a client 
* requirements:
*       1. ftserver starts on host A and validates command-line parameters (<SERVER_PORT>)
*       2. ftserver waits on <PORTNUM> for a client request
*       3. ftclient starts on Host B, and validates any pertinent command-line parameters 
	(<SERVER_HOST>, <SERVER_PORT>, <COMMAND>, <FILENAME>, <DATA_PORT>, etc...)
*       4. ftserver and ftclient establist a TCP 'control' connection on <SERVER_PORT>. (call this connection C)
*	5. ftserver waits on connection P for ftclient to send command
*	6. ftclient sends command (-l (list) or -g <FILENAME> (get)) on connection C
*	7. ftserver receives command on connection C
		* if ftclient sent an invalid command:
			* ftserver sends an error message to ftclient on connection C, and
			ftclient displays the message on-screen
		* otherwise:
			* ftserver initiates a TCP 'data' connection with ftclient on
			<DATA_PORT> (call this connection D)
			* if ftclient has sent the -l command, ftserver sends its directory to 
			ftclient on connection D, and ftclient displays the directory on-screen
			* if ftclient has sent -g <FILENAME> command, ftserver validates FILENAME
			and either:
				* sends the contents of FILENAME on connection D, ftclient saves
				the file in the current default directory (handling "duplicate file
				name" error if necessary) and displays a "transfer complete" message
				on-screen
			* or:
				* sends an appropriate error message ("File not found", etc.) to ftclient
				on connection C and ftclient displays the message on-screen
			* ftserver closes connection D (don't leave open sockets!)
* 	8. ftclient closes connection C (don't leave open sockets!)
*	9. ftserver repeats from 2 (above) until terminated by a supervisor (SIGINT)  

        * ftclient is implemented in python
        * ftserver is implemented in c
* references:
       * see the included README.txt file for documentation of the program (chatclient.c)
*/

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define c_portno_min	1024
#define c_portno_max	65535


void checkArgs(int agrc, char** argv, int s_portno);
int checkPortArgInt(char* portArg, int* s_portno);
void ftp(int s_portno);

int main(int argc, char** argv){
	int s_portno;
	checkArgs(argc, argv, s_portno);
	

	return 0;
}

/* void checkArgs function: 
	* inputs:
		* int argc -- holds the number of command line argument passed in by user
		* char** argv -- a pointer to pointers of type char -- an array of char pointers (c strings) -- an array of command line arguments
		* int s_portno -- holds the <SERVER_PORT> upon conversion to integer
	* outputs:
		* on success -- void
		* on failure -- exit(1)
	* calls: 
		* int checkPortArg(char* portArg, int* s_portno)
	* purpose:
		* checks the number of command line arguments
		* checks that argument 1 <SERVER_PORT> is an integer 
		* checks that argument 1 <SERVER_PORT> is within range
*/
void checkArgs(int argc, char** argv, int s_portno){
	if(argc != 2){
		fprintf(stderr, "usage: ./ftserver <SERVER_PORT>\n");
		exit(1);
	}
	
	// check if s_portno argument is an integer
	if(!checkPortArgInt(argv[1], &s_portno)){
		fprintf(stderr, "error: ftserver port number must be an integer\n");
		exit(1);
	}

	// check s_portno range [1024, 65535]
	if(s_portno < c_portno_min || s_portno > c_portno_max){
		fprintf(stderr, "error: ftserver port number must be between 1024-65535\n");
		exit(1);
	}

}

/* int checkPortArg function: 
	* inputs:
		* char* portArg -- holds the <SERVER_PORT> argument from command line
		* int* s_portno -- pointer to variable that to hold <SERVER_PORT> arugment from command line upon conversion to integer
	* outputs:
		* returns 1 if portArg is an integer; returns 0 if portArg is not an integer
	* purpose:
		* to ensure that the command line argument, portArg, is an integer and therefore can be assigned to the server socket
*/
int checkPortArgInt(char* portArg, int* s_portno){
	// this var holds a potential trailing non-whitespace char
	char strCheck;
	
	// sscanf returns the number of variables filled; in case of input failure before any data could be successfully read, EOF is returned
	// in this case, if portno argument is an integer value 1 is returned (1 int variable filled) else 0 returned (0 variables filled)
	int matches = sscanf(portArg, "%d %c", s_portno, &strCheck);

	return matches == 1;
}

/* void ftp(int s_portno)
	* inputs:
		* int s_portno -- holds the <SERVER_PORT> argument from command line upon conversion to integer
	* outputs:
		* on success -- 
		* on failure -- perrer(<error_message>) exit(1)
	* calls:
		* 
	* purpose:
		* create server socket; bind server socket with server address and server port number
		* listen for incoming connections on socket
		* provide FTP for client connections
			* create/maintain Control connection
			* create Data connection
		* end FTP on interrupt signal
*/
void ftp(int s_portno){
	//file descriptor for server socket
	int s_sockfd;
	//server socket address
	//The variable serv_addr is a structure of type struct sockaddr_in. This structure has four fields
	struct sockaddr_in serv_addr;
	
		
	//creating the socket
	//arg1 = domain (AF_INET for IP); arg2 = transport protocol type (UDP = SOCK_DGRAM, TCP = SOCK_STREAM); arg3 = protocol (IP = 0)
	//check if socket was created
	if((s_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket creation");
		exit(1);
	}

	// --initialize serv_addr struct: 4 fields -- //

	//zero out serv_addr
	bzero((char*) &serv_addr, sizeof(serv_addr));	

	//The first field is short sin_family, which contains a code 
	//for the address family. It should always be set to the symbolic constant AF_INET: IPv4
	serv_addr.sin_family = AF_INET;
	
	//set port number of socket to portno arg
	//The second field is an unsigned short: sin_port
	//The htons(hostshort) function converts the unsigned short integer hostshort from host byte order to network byte order (i.e., Big Endian conversion)
	serv_addr.sin_port = htons(s_portno);
	
	//The third field is a structure of type struct in_addr which contains only a single field: unsigned long s_addr
	//field: unsigned long s_addr contains the IP address of the host; for a serverthis will always be the IP address of the machine on which the server is running, and there
	//is a symbolic constant INADDR_ANY which gets the address of localhost
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	//bind server socket with host address and server portno
	int status = bind(s_sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));


}

