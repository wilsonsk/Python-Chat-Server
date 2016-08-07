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
#define MAX_CXN_ATTEMPTS 12 // Arbitrary number of connection requests

void intSigHandler(int sig);
int checkPortArgInt(char *str, int *n);
char **listFiles(char *dirname, int *numFiles);
void recvFile(int socket, void *buffer, int size);
void recvPack(int socket, char *tag, char *data);
int controlConnection(int controlSocket, char *commandTag, int *dataPort, char* filename);
int dataConnection(int controlSocket, int dataSocket, char *commandTag, char *filename);
void sendFile(int socket, void *buffer, int numBytes);
void sendPack(int socket, char *tag, char *data);
void ftp(int port);

int main(int argc, char **argv)
{
	int port;  // Port number on which to listen for client connections.

	// Exactly two command-line arguments are expected.
	if (argc != 2) {
		fprintf(stderr, "usage: ftserver <server-port>\n");
		exit(1);
	}

	// The given port number must be an integer.
	if (!checkPortArgInt(argv[1], &port)) {
		fprintf(stderr, "ftserver: Port number must be an integer\n");
		exit(1);
	}

	// The given port number must be in the range [c_portno_min, c_portno_max].
	if (port < c_portno_min || port > c_portno_max) {
		fprintf(stderr, "ftserver: Port number must be in the range [1024, 65535]\n");
		exit(1);
	}

	// Run the FTP server until an interrupt signal is detected.
	ftp(port);

	exit(0);
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
void intSigHandler(int sig)
{
	int status;                   // Return status
	struct sigaction interrupt;   // Signal action for handling interrupt

	// Provide feedback to the user.
	printf("\nftserver closed\n");

	//restore signal (interrupt) handling to default action via SIG_DFL
	interrupt.sa_handler = SIG_DFL;
	status = sigaction(SIGINT, &interrupt, 0);
	if (status == -1) {
		perror("sigaction");
		exit(1);
	}

	//send an interrupt signal to force default action -- to be acted on by the newly set default action for this signal
	status = raise(SIGINT);
	if (status == -1) {
		perror("raise");
		exit(1);
	}
}


/* int checkPortArgInt function: 
	* inputs:
		* char* portArg -- holds the <SERVER_PORT> argument from command line
		* int* s_portno -- pointer to variable that holds <SERVER_PORT> arugment from command line upon conversion to integer
	* outputs:
		* returns 1 if portArg is an integer; returns 0 if portArg is not an integer
	* purpose:
		* to ensure that the command line argument, portArg, is an integer and therefore can be assigned to the server socket
*/
int checkPortArgInt(char *str, int *n)
{
	// this var holds a potential trailing non-whitespace char
	char c;

	// sscanf returns the number of variables filled; in case of input failure before any data could be successfully read, EOF is returned
	// in this case, if portno argument is an integer value 1 is returned (1 int variable filled) else 0 returned (0 variables filled)
	int matches = sscanf(str, "%d %c", n, &c);

	// A string representation of an integer will result in only one match.
	return matches == 1;
}


/* char** listFiles(char* dirname, int* numFiles)
	* inputs:
		* char* dirname -- c string directory name
		* int* numFiles -- number of files in the <dirname> directory
	* outputs:
		* char* array of filenames of <dirname> directory
	* calls:
		* DIR* opendir(const char* dirname)
			* open a directory stream corresponding to the directory named by the dirname
			arugment.The directory stream is positioned at the first entry. If the type of DIR
			is implemented using a file descriptor, apps shall only be able to open up to a 
			total of {OPEN_MAX} files and directories.
			* returns a pointer to an object of type DIR; otherwise a null pointer 
			shall be returned and errno set to indicate the error
		* DIR* closedir(DIR* dirp)
			* shall close the directory stream referred to by the argument dirp. Upon
			return, the value of dirp may no longer point to an accessible object of the
			type DIR. If a file descriptor is used to implement type DIR, that file descriptor 
			shall be closed.
			* returns 0 on success; -1 on failure
		* struct dirent* readdir(DIR* dirp)
			* returns a pointer to an object of type struct dirent on success; null pointer 
			and errno set on error
		* int stat(const char* restrict path, struct stat* restrict buf)
			* shall obtain info about the named fil and write it to the area pointed to by the
			buf argument.
			* path -- points to a pathname naming a file
			* buf -- is a pointer to a stat structure, into which info is placed concerning the
			file
		* S_ISDIR(mode_t m) 
			* MACRO returns non-zero if the file is a directory
	* purpose:
		* lists all files in the specified directory
*/
char ** listFiles(char *dirname, int *numFiles)
{
	char **fileList;      // Return value
	DIR *dir;             // Directory object
	struct dirent *entry; // Entry within a directory
	struct stat info;     // Information concerning a directory entry

	// Open the given directory.
	dir = opendir(dirname);
	if (dir == NULL) {
		fprintf(stderr, "ftserver: unable to open %s\n", dirname);
		exit(1);
	}

	// Build a list of filenames in the given directory.
	*numFiles = 0;
	fileList = NULL;
	while ((entry = readdir(dir)) != NULL) {

		// Skip subdirectories.
		stat(entry->d_name, &info);
		if (S_ISDIR(info.st_mode)) {
			continue;
		}

		// Append current filename to the list.
		{
			// Allocate memory for the new item.
			if (fileList == NULL) {
				fileList = malloc(sizeof(char *));
			} else {
				fileList = realloc(fileList, (*numFiles + 1) * sizeof(char *));
			}
			assert(fileList != NULL); // malloc()/realloc() failure check
			fileList[*numFiles] = malloc((strlen(entry->d_name) + 1) * sizeof(char));
			assert(fileList[*numFiles] != NULL); // malloc() failure check

			// Store the filename in the list.
			strcpy(fileList[*numFiles], entry->d_name);

			// Update the list length.
			(*numFiles)++;
		}
	}

	// Cleanup.
	closedir(dir);

	return fileList;
}


/* void recvFile(int sockfd, void* buf, int size)
	* inputs: 
		* int sockfd -- file descriptor of the socket connection to be used for FTP
		* void* buf -- this is a generic pointer type which can be converted to any other pointer type without explicit cast: must convert to complete data type before dereferencing or pointer arithmetic
			    -- this buffer will be used to store the client's data
		* int size -- pre determined number of bytes to accept/receive from client
	* outputs:	
		* on success -- client data stored in void* buf
		* on failure -- perror(<error_message>) exit(1)
	* calls:
		* ssize_t recv(int sockfd, void* buf, size_t len, int flags) 
			* sockfd -- specifies the socket file descriptor
			* buffer -- points to a buffer where the message should be stored
			* length -- specifies the length in bytes of the buffer pointed to by the buffer argument
			* flags -- specifies the type of message reception. Values of this argument are formed by logically OR'ing zero or more of specific values 
			* returns -- on success number of bytes received ; on failure -1
				* more data may be coming as long as the return value is greater than 0
				* recv will block if the connection is open but no data is available
				* this function is called until return value is 0 (i.e., all data received from client)
	* purpose:
		* receive client data (either client command of filename to retrieve)
*/
void recvFile(int socket, void *buffer, int numBytes)
{
	int ret;               // Return value for 'recv'
	int receivedBytes;     // Total number of bytes received

	//receive passed in number of bytes from client
	receivedBytes = 0;
	while (receivedBytes < numBytes) {
		ret = recv(socket, buffer + receivedBytes, numBytes - receivedBytes, 0);

		// Error encountered.
		if (ret == -1) {
			perror("recv");
			exit(1);
		}

		// Data received.
		else {
			receivedBytes += ret;
		}
	}
}


/* void recvPack(int sockfd, char* option, char* data){
	* inputs:
		* int sockfd -- file descriptor of socket connection to be used for communication
		* char* option -- a string that holds an option from client command line to be received
		* char* data -- a string that holds the data from the client command line to be received
	* outputs:
		* char* option, char* data modified and now hold data read in from client
	* calls:
		* recvFile()
	* purpose:
		* receives data packets from given client socket connection: ref: Beej's Guide to Network Programming: section 7.5, page 53
			* len (1 byte, unsigned) -- total length of the packet, counting the 8-byte command option (-l || -g) and n-bytes data
			* command option (8-bytes)
			* client data (n-bytes)

*/	
void recvPack(int socket, char *tag, char *data)
{
	unsigned short packetLength;       // Number of bytes in packet
	unsigned short dataLength;         // Number of bytes in encapsulated data
	char tmpTag[ARG_LEN + 1];          // Temporary tag transfer buffer
	char tmpData[MAX_PACK_PAYLOAD_LEN + 1]; // Temporary payload transfer buffer

	//read in client packet size: packLen passed by reference because this var will be modified within the recvFile()
	recvFile(socket, &packetLength, sizeof(packetLength));
	//ntohs(uint16_t netshort): function that converts the unsigned short integer called netshort from network byte (Big Endian) order to host byte order (little Endian)
	packetLength = ntohs(packetLength);

	//read in client command 
	recvFile(socket, tmpTag, ARG_LEN);
	//set string terminator character to end of optionBuf to mark end of option string
	tmpTag[ARG_LEN] = '\0';
	if (tag != NULL) { strcpy(tag, tmpTag); }

	//receive data payload from client: dataLen = total packet size - command option - sizeof packLen
	dataLength = packetLength - ARG_LEN - sizeof(packetLength);
	recvFile(socket, tmpData, dataLength);
	//set string terminator character to end of dataBuf to mark end of data payload string
	tmpData[dataLength] = '\0';
	if (data != NULL) { strcpy(data, tmpData); }
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
		* recvPack(int sockfd, char* cmdArg, char* fileArg)
		* sendPack()
	* purpose:
		* read in from client and store command arugments in appropriate static arrays 
		* send feedback to client 
		* establish && maintain basic control connection between server and client
*/
int controlConnection(int controlSocket, char *commandTag, int *dataPort, char* filename)
{
	char indata[MAX_PACK_PAYLOAD_LEN + 1];  	//holds read in client <FILENAME> argument -- input packet payload
	char intag[ARG_LEN + 1];           			//holds read in client <COMMAND> argument
	char outdata[MAX_PACK_PAYLOAD_LEN + 1]; 	// Output packet payload
	char outtag[ARG_LEN + 1];          			

	//read in data connection port from client
	printf("  Receiving data port (FTP active mode) ...\n");
	recvPack(controlSocket, intag, indata);
	//if the command line option held in readinCommand == DPORT, then convert string held in readinDataPort to integer and assign to dataConnPort
	if (strcmp(intag, "DPORT") == 0) { *dataPort = atoi(indata); }

	//read in command from client
	printf("  Receiving command ...\n");
	recvPack(controlSocket, intag, indata);
	strcpy(commandTag, intag);
	strcpy(filename, indata);

	//error check
	if (strcmp(intag, "LIST") != 0 && strcmp(intag, "GET") != 0) {
		printf("  Transmitting command error ...\n");
		strcpy(outtag, "ERROR");
		strcpy(outdata, "Command must be either -l or -g");
		sendPack(controlSocket, outtag, outdata);
		return -1;
	}

	// Otherwise, indicate that it is okay to establish an FTP data connection.
	else {
		printf("  Transmitting data-connection go-ahead ...\n");
		strcpy(outtag, "OKAY");
		sendPack(controlSocket, outtag, "");
		return 0;
	}
}


/* int dataConnection(int controlfd, int datafd, char* commandAgr, char* filename)
	* inputs:
		* int controlfd -- file descriptor of control socket connection
		* int datafd -- file descriptor of data socket connection
		* char* commandArg -- c string that holds the client's command argument
		* char* filename -- c string that holds the client's requested filename
	* outputs:
		* on success -- 0
		* on failure -- -1
	* calls:
		* listFiles()
		* sendPack()
		* fopen(filename, "r/w/rw")
		* fread()
	* purpose:
		* allow file transfer between server and client
*/
int dataConnection(int controlSocket, int dataSocket, char *commandTag, char *filename)
{
	int ret = 0;     // Return value
	char **fileList; // List of filenames in the current directory
	int numFiles;    // Number of files the current directory
	int i;		//for loop iterator

	// Get a list of filenames in the current directory.
	fileList = listFiles(".", &numFiles);

	//check client's command argument
	if (strcmp(commandTag, "LIST") == 0) {

		// Transfer each filename within a separate packet.
		printf("  Transmitting file listing ...\n");
		for (i = 0; i < numFiles; i++) {
			sendPack(dataSocket, "FNAME", fileList[i]);
		}
	}

	// The client requests transmission of a file.
	else if (strcmp(commandTag, "GET") == 0) {
		do {
			char buffer[MAX_PACK_PAYLOAD_LEN + 1]; // File reader storage buffer
			int bytesRead;  // Number of bytes read from a file
			int fileExists; // Flag indicating if given filename exists in list
			FILE *infile;   // Reference to input file

			//search the files in current dir
			fileExists = 0;
			for (i = 0; i < numFiles && !fileExists; i++) {
				if (strcmp(filename, fileList[i]) == 0) {
					fileExists = 1;
				}
			}

			//check if <FILENAME> is in current directory
			if (!fileExists) {
				printf("  Transmitting missing-file error ...\n");
				sendPack(controlSocket, "ERROR", "File not found");
				ret = -1;
				break;
			}

			//open file 
			infile = fopen(filename, "r");
			if (infile == NULL) {
				printf("  Transmitting file-read-access error ...\n");
				sendPack(controlSocket, "ERROR", "Unable to open file");
				ret = -1;
				break;
			}

			//FT the filename
			sendPack(dataSocket, "FILE", filename);

			//FT the file 
			printf("  Transmitting file ...\n");
			do {
				bytesRead = fread(buffer, sizeof(char), MAX_PACK_PAYLOAD_LEN, infile);
				buffer[bytesRead] = '\0';
				sendPack(dataSocket, "FILE", buffer);
			} while (bytesRead > 0);
			if (ferror(infile)) {
				perror("fread");
				ret = -1;
			}
			fclose(infile);

		} while (0);
	}

	// Given command-tag must be either "LIST" or "GET".
	else {
		fprintf(stderr, "ftserver: command-tag must be \"LIST\" or "
		        "\"GET\"; received \"%s\"\n", commandTag            );
		ret = -1;
	}

	//place done tag at end of data to indicate FTP complete 
	sendPack(dataSocket, "DONE", "");

	// Inform the client that the control connection can be closed.
	printf("  Transmitting connection-termination go-ahead ...\n");
	sendPack(controlSocket, "CLOSE", "");

	// Cleanup.
	for (i = 0; i < numFiles; i++) {
		free(fileList[i]);
	}
	free(fileList);

	return ret;
}


/* void sendFile(int sockfd, void* buffer, int presetSize){
	* inputs:
                * int sockfd -- file descriptor of the socket connection to be used for FTP
                * void* buf -- this is a generic pointer type which can be converted to any other pointer type without explicit cast: must convert to complete data type before dereferencing or pointer arithmetic
                            -- this buffer will be used to store the client's data
                * int size -- pre determined number of bytes to accept/receive from client
	* outputs:
		* none
	* calls:
		* send() -- shall initiate transmission of a message from the specified socket to its peer'
			* returns number of bytes sent on success, -1 on failure 
	* purpose:
		* send data until bytes sent is = to the preset size

*/
void sendFile(int socket, void *buffer, int numBytes)
{
	int ret;           //holds return value of send()
	int sentBytes;     // Total number of bytes sent

	// Send the given number of bytes.
	sentBytes = 0;
	while (sentBytes < numBytes) {
		ret = send(socket, buffer + sentBytes, numBytes - sentBytes, 0);

		// Error encountered.
		if (ret == -1) {
			perror("send");
			exit(1);
		}

		// Data sent.
		else {
			sentBytes += ret;
		}
	}
}


/* void sendPack(int sockfd, char* option, char* data);
	* intputs: 
		* int sockfd -- file descriptor for socket connection
		* char* option -- c string for client command option
		* char* data -- c string for client data payload
	* outputs:
		* none
	* calls:
		* sendFile()
	* purpose:
		* sends a packet from the specified socket

*/
void sendPack(int socket, char *tag, char *data)
{
	unsigned short packetLength;        // Number of bytes in packet
	char tagBuffer[ARG_LEN];            // Transmission buffer for given tag

	//send packLen
	packetLength = htons(sizeof(packetLength) + ARG_LEN + strlen(data));
	sendFile(socket, &packetLength, sizeof(packetLength));

	//send command option
	memset(tagBuffer, '\0', ARG_LEN);   // Null-padding
	strcpy(tagBuffer, tag);
	sendFile(socket, tagBuffer, ARG_LEN);

	//send data
	sendFile(socket, data, strlen(data));
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
void ftp(int port)
{	
	//file descriptor for server socket
	int serverSocket;                 // Socket for receiving client requests
	int status;                       // Return status
	//handles interrupt signals
	struct sigaction interrupt;       // Signal action for handling interrupt
	struct sockaddr_in serverAddress; // Server address

	// Configure the server address.
	//The first field is short sin_family, which contains a code 
	//for the address family. It should always be set to the symbolic constant AF_INET: IPv4
	serverAddress.sin_family = AF_INET;         // IPv4
	
	//set port number of socket to portno arg
	//The second field is an unsigned short: sin_port
	//The htons(hostshort) function converts the unsigned short integer hostshort from host byte order to network byte order (i.e., Big Endian conversion)
	serverAddress.sin_port = htons(port);       // Big-endian conversion
	
	//The third field is a structure of type struct in_addr which contains only a single field: unsigned long s_addr
	//field: unsigned long s_addr contains the IP address of the host; for a serverthis will always be the IP address of the machine on which the server is running, and there
	//is a symbolic constant INADDR_ANY which gets the address of localhost
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Localhost

	// Create a server-side socket.
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1) {
		perror("socket");
		exit(1);
	}
	
	//bind server socket with host address and server portno
	//int status is an integer that holds return value of bind, listen, connect socket functions for error checking
	// Associate server-side socket with listening port.
	status = bind(serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress));
	if (status == -1) {
		perror("bind");
		exit(1);
	}

	//listen for incoming connections
	//listen() puts the socket into server mode
	//Listen for connections made to the socket. The backlog argument specifies the maximum number of queued connections and should be at least 0; 
	//the maximum value is system-dependent (usually 5), the minimum value is forced to 0 --  this is a global variable defined a top of document
	status = listen(serverSocket, BACKLOG);
	if (status == -1) {
		perror("listen");
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
	interrupt.sa_handler = &intSigHandler;
	interrupt.sa_flags = 0;
	// int sigemptyset(sigset_t* set): initializes the signal set pointed to by parameter 'set', such that all signals signals defined in this document are excluded
	// returns 0 on success; -1 on failure (errno set)
	sigemptyset(&interrupt.sa_mask);
	// SIGINT: signal interrupt
	// 1st parameter: setting interrup signal as signal of interest (i.e., signal that triggers handler)
	// 2nd parameter: pointer to the specific sigaction structure which was initialized above
	// 3rd parameter: pointer to another sigaction structure (in this case it is not used; set to 0)
	status = sigaction(SIGINT, &interrupt, 0);
	if (status == -1) {
		perror("sigaction");
		exit(1);
	}

	// Provide FTP services to clients until interrupted.
	printf("ftserver: FTP server open on port %d\n", port);
	while (1) {
		char *clientIPv4;                   // Client dotted-decimal address
		char commandTag[ARG_LEN + 1];       // Buffer to store command tag
		char filename[MAX_PACK_PAYLOAD_LEN + 1]; // Buffer to store filename
		int controlSocket, dataSocket;      // Server-side FTP connection endpoints
		int dataPort;                       // Client-side data connection port
		socklen_t addrLen;                  // Length of an address struct
		struct sockaddr_in clientAddress;   // Client address

		// Establish FTP control connection.
		addrLen = sizeof(struct sockaddr_in);
		controlSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &addrLen);
		if (controlSocket == -1) {
			perror("accept");
			exit(1);
		}
		//inet_ntoa(struct in_addr in): converts the internet host address called 'in' (clientIP) given in network byte order(Big Endian), to a string in IPv4 dotted-decimal notation. 
		//the string is returned in a statically allocated buffer which subsequent calls will overwrite
		clientIPv4 = inet_ntoa(clientAddress.sin_addr);
		printf("\nftserver: FTP control connection established with \"%s\"\n", clientIPv4);

		// Communicate over FTP control connection.
		status = controlConnection(controlSocket, commandTag, &dataPort, filename);

		// Provide FTP data services if control session was successful.
		if (status != -1) {
			int connectionAttempts;  // Number of data connection requests

			// Create server-side endpoint of FTP data connection.
			dataSocket = socket(AF_INET, SOCK_STREAM, 0);
			if (dataSocket == -1) {
				perror("socket");
				exit(1);
			}

			// Establish FTP data connection.
			clientAddress.sin_port = htons(dataPort);
			connectionAttempts = 0;
			do {
				status = connect(dataSocket, (struct sockaddr *) &clientAddress, sizeof(clientAddress));
			} while (status == -1 && connectionAttempts < MAX_CXN_ATTEMPTS);
			if (status == -1) {
				perror("connect");
				exit(1);
			}
			printf("ftserver: FTP data connection established with \"%s\"\n", clientIPv4);

			// Transfer file information over FTP data connection.
			dataConnection(controlSocket, dataSocket, commandTag, filename);

			// Wait for client to acknowledge received data.
			recvPack(controlSocket, NULL, NULL);

			// Close FTP data connection.
			status = close(dataSocket);
			if (status == -1) {
				perror("close");
				exit(1);
			}
			printf("ftserver: FTP data connection closed\n");
		}
	}
}
