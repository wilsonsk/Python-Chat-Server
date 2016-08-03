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


void checkArgs(int agrc, char** argv, int s_portno);
int checkPortArgInt(char* portArg, int* s_portno);

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
		* 
	* purpose:
		* 
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
	if(s_portno < 1024 || s_portno > 65535){
		fprintf(stderr, "error: ftserver port number must be between 1024-65535\n");
		exit(1);
	}

}

/* int checkPortArg function: 
	* inputs:
		* char* portArg -- holds the <SERVER_PORT> argument from command line
		* int* s_portno -- pointer to variable that to hold <SERVER_PORT> upon conversion to integer
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




