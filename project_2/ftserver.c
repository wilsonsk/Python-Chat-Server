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

#define c_portno_min		1024
#define c_portno_max		65535
#define ARG_LEN			8	//number of bytes for command line <COMMAND> argument (-l or -g)
#define MAX_PACK_PAYLOAD_LEN	512	//max number of bytes in packet payload

//The backlog argument specifies the maximum number of queued connections and should be at least 0; 
//the maximum value is system-dependent (usually 5), the minimum value is forced to 0
#define BACKLOG		5



void checkArgs(int agrc, char** argv, int* s_portno);
int checkPortArgInt(char* portArg, int* s_portno);
void ftp(int s_portno);
void intSigHandler(int signal);
int controlConnection(int c_controlfd, char* commandArg, int* dataConnPort, char* filename);

int main(int argc, char** argv){
	int s_portno;
	checkArgs(argc, argv, &s_portno);
	ftp(s_portno);	

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
void checkArgs(int argc, char** argv, int* s_portno){
	if(argc != 2){
		fprintf(stderr, "usage: ./ftserver <SERVER_PORT>\n");
		exit(1);
	}
	
	// check if s_portno argument is an integer
	if(!checkPortArgInt(argv[1], s_portno)){
		fprintf(stderr, "error: ftserver port number must be an integer\n");
		exit(1);
	}

	// check s_portno range [1024, 65535]
	if(*s_portno < c_portno_min || *s_portno > c_portno_max){
		fprintf(stderr, "error: ftserver port number must be between 1024-65535\n");
		exit(1);
	}

}

/* int checkPortArg function: 
	* inputs:
		* char* portArg -- holds the <SERVER_PORT> argument from command line
		* int* s_portno -- pointer to variable that holds <SERVER_PORT> arugment from command line upon conversion to integer
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
		* void intSigHandler(int signal)
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
	//int status is an integer that holds return value of bind, listen, connect socket functions for error checking
	int status = bind(s_sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	if(status == -1){
		perror("socket binding");
		exit(1);
	}
	
	//listen for incoming connections
	//listen() puts the socket into server mode
	//Listen for connections made to the socket. The backlog argument specifies the maximum number of queued connections and should be at least 0; 
	//the maximum value is system-dependent (usually 5), the minimum value is forced to 0 --  this is a global variable defined a top of document
	status = listen(s_sockfd, BACKLOG);
	if(status == -1){
		perror("server listen");
		exit(1);
	}
	
	
	//use sigaction() to register a signal handling function (that I've created for a specific set of signals)
	//int sigaction(int signo, struct sigaction* newaction, struct sigaction* origaction) -- this is a pointer function field within the sigaction structure
		//1st parameter: the signal type of interest (i.e., that triggers a specific handling function)
		//2nd parameter: a pointer to a special sigaction structure -- describes the action to be taken upon receipt of signal (1st parameter)
			//this sigaction structure has a field called sa.handler (a pointer to a function) and it is assigned the address of the handler function
		//3rd parameter: a pointer to another sigaction structure -- the sigaction() function will use this pointer to write out what the settings were before the change was requested
		// returns 0 on success; -1 on failure -- (this is an assumption)
	//struct sigaction{ void (*sa_handler)(int); sigset_t sa_mask; int sa_flags; void (*sa_sigaction)(int, siginfo_t*, void*); -- sigset_t is a signal set (a list of signal types)
		// void (*sa_handler)(int): 3 possible values:
			// SIG_DFL -- take the default action
			// SIG_IGN -- ignore the signal
			// &<someHandlerFunction> -- a pointer to a function that should be called when this signal is received
		// sigset_t sa_mask: includes what signals should be blocked while the signal handler is executing
			// blocked means that the signals are put on hold until your signal handler is done executing
			// you need to pass this a signal set
		// int sa_flags:  provides additional options (flags)
			// SA_RESTHAND -- resets the signal handler to SIG_DFL (default action) after the first signal has been received and handled 
			// SA_SIGINFO -- tells the kernel to call the function specified in the sigaction struct's 4th field (sa_sigaction), instead of the first field (sa_handler)
				//more detailed information is passed to this function
			// make sure that you set sa_flags to 0 if you aren't planning to set any flags
		// void(*sa_sigaction)(int, siginfo_t*, void*)
			// *sig_action specifies an alternative signal handler function to be called; this attribute will only be used if the SA_SIGINFO flag is set in sa_flags
			// the signinfo_t structure contains information such as which process sent you the signal
			// most of the time you will use sa_handler and not sa_sigaction

	//handles interrupt signals
	struct sigaction interrupt;
	//set handler function to be called (via function pointer)
	interrupt.sa_handler = &intSigHandler;
	//setting NO FLAGS (i.e., 0)
	interrupt.sa_flags = 0;	

	// int sigemptyset(sigset_t* set): initializes the signal set pointed to by parameter 'set', such that all signals signals defined in this document are excluded
		// returns 0 on success; -1 on failure (errno set)
	sigemptyset(&interrupt.sa_mask);
	
	// SIGINT: signal interrupt
		// 1st parameter: setting interrup signal as signal of interest (i.e., signal that triggers handler)
		// 2nd parameter: pointer to the specific sigaction structure which was initialized above
		// 3rd parameter: pointer to another sigaction structure (in this case it is not used; set to 0)
	status = sigaction(SIGINT, &interrupt, 0);
	if(status == -1){
		perror("sigaction");
		exit(1);
	}
		

	//initiate FTP services upon client connection to server socket
	printf("Server open on %d\n", serv_addr.sin_port);
	while(1){
		// initialize client socket vars
		char* clientIP;		//holds client IPv4 dd address
		socklen_t clilen;	//length of sockaddr_in struct client_addr 
		struct sockaddr_in client_addr;
		int c_controlfd, c_datafd;
		int dataConnPort;

		//ftp vars
		char commandArg[ARG_LEN + 1];			//buffer for <COMMAND> arg (-l || -g)
		char filename[MAX_PACK_PAYLOAD_LEN + 1];	//buffer for <FILENAME> arg

		//create FTP control connection
		clilen = sizeof(client_addr);
		c_controlfd = accept(s_sockfd, (struct sockaddr*) &client_addr, &clilen);
		if(c_controlfd == -1){
			perror("accept");
			exit(1);
		}

		//inet_ntoa: converts the internet host address (clientIP) given in network byte order, to a string in IPv4 dotted-decimal notation. 
			//the string is returned in a statically allocated buffer which subsequent calls will overwrite
		clientIP = inet_ntoa(client_addr.sin_addr);
		printf("control connection established with %s\n", clientIP);
		
		//enable/maintain basic communication via control connection with client
		status = controlConnection(c_controlfd, commandArg, &dataConnPort, filename);

		//once control connection maintained, start FTP services
	
	}	

}

/* void intSigHandler(int signal)
        * inputs:
                * int signal -- holds the signal number/id
        * outputs:
                * on success -- void -- restore signal handling to default behavior; send an interrup signal to force default behavior
                * on failure -- perrer(<error_message>) exit(1)
        * calls:
		* sigaction(int signo, struct sigaction* newaction, struct sigaction* origaction)
		* raise(int signal)
        * purpose:
		* callback function
		* displays feedback before terminating ftserver program due to interrupt signal
		* restore signal handling to default action; send an interrup signal to force default action
			* via setting *sa_handler = SIG_DFL which directs handler to take default action

*/
void intSigHandler(int signal){
	int status;
	struct sigaction interrupt;

	printf("ftserver closed due to interrupt signal\n");

	//restore signal (interrupt) handling to default action via SIG_DFL
	interrupt.sa_handler = SIG_DFL;
	status = sigaction(SIGINT, &interrupt, 0);
	if(status == -1){
		perror("sigaction in handler");
		exit(1);
	}
	
	//send an interrupt signal to force default action -- to be acted on by the newly set default action for this signal
	status = raise(SIGINT);
	if(status == -1){
		perror("raise");
		exit(1);
	}
}

/* int controlConnection(int c_controlfd, char* commandArg, int* dataConnPort, char* filename)
	* inputs:
		* int c_controlfd -- file descriptor of control socket
		* char* commandArg -- pointer/c string (static array) holding <COMMAND> argument from command line (-l || -g) 
		* int* dataConnPort -- holds data connection port file descriptor
		* char* filename -- pointer/c string (static array) holding <FILENAME> argument from command line
	* outputs:
		* 0 on success
			* modifies commandArg, dataConnPort, filename
		* -1 on failure
	* calls:
		* recvPack()
		* sendPack()
	* purpose:
		* read in from client and store command arugments in appropriate static arrays 
		* send feedback to client 
		* establish && maintain basic control connection between server and client
*/
int controlConnection(int c_controlfd, char* commandArg, int* dataConnPort, char* filename){
	char readinFilename[MAX_PACK_PAYLOAD_LEN + 1];		//holds read in client <FILENAME> argument -- input packet payload
	char readinCommand[ARG_LEN + 1];			//holds read in client <COMMAND> argument
	char readoutData[MAX_PACK_PAYLOAD_LEN + 1];
	char readoutArg[ARG_LEN + 1];

	//read in data from client
	
}
