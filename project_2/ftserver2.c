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

#define BACKLOG           5 // Arbitrary queue size for connection requests
#define MAX_CXN_ATTEMPTS 12 // Arbitrary number of connection requests
#define MAX_PAYLOAD_LEN 512 // Maximum number of bytes in packet payload
#define TAG_LEN           8 // Number of bytes reserved for tag field

void handleInterrupt(int sig);
int isStrInt(char *str, int *n);
char **listFiles(char *dirname, int *numFiles);
void recvAll(int socket, void *buffer, int size);
void recvPacket(int socket, char *tag, char *data);
int runControlSession(int controlSocket, char *commandTag, int *dataPort, char* filename);
int runDataSession(int controlSocket, int dataSocket, char *commandTag, char *filename);
void sendAll(int socket, void *buffer, int numBytes);
void sendPacket(int socket, char *tag, char *data);
void startFtpServer(int port);

int main(int argc, char **argv)
{
	int port;  // Port number on which to listen for client connections.

	// Exactly two command-line arguments are expected.
	if (argc != 2) {
		fprintf(stderr, "usage: ftserver <server-port>\n");
		exit(1);
	}

	// The given port number must be an integer.
	if (!isStrInt(argv[1], &port)) {
		fprintf(stderr, "ftserver: Port number must be an integer\n");
		exit(1);
	}

	// The given port number must be in the range [1024, 65535].
	if (port < 1024 || port > 65535) {
		fprintf(stderr, "ftserver: Port number must be in the range [1024, 65535]\n");
		exit(1);
	}

	// Run the FTP server until an interrupt signal is detected.
	startFtpServer(port);

	exit(0);
}


/*
 * Function: handleInterrupt
 *
 * Purpose:  This callback provides feedback before terminating the 'ftserver'
 *           program in response to an interrupt signal.
 *
 * Receives: sig - signal number
 *
 * Returns:  None
 *
 * Pre:      None
 *
 * Post:     The program terminates.
 */
void handleInterrupt(int sig)
{
	int status;                   // Return status
	struct sigaction interrupt;   // Signal action for handling interrupt

	// Provide feedback to the user.
	printf("\nftserver closed\n");

	// Restore interrupt handling to the default behavior.
	interrupt.sa_handler = SIG_DFL;
	status = sigaction(SIGINT, &interrupt, 0);
	if (status == -1) {
		perror("sigaction");
		exit(1);
	}

	// Send an interrupt signal in order to force the default behavior.
	status = raise(SIGINT);
	if (status == -1) {
		perror("raise");
		exit(1);
	}
}


/*
 * Function: isStrInt
 *
 * Purpose:  This function determines whether or not a string represents an
 *           integer.
 *
 * Receives: str - c-string that may represent an integer
 *           n   - location in which to store the result of numeric conversion
 *
 * Returns:  Whether or not the string is an integer is returned.
 *
 * Pre:      None
 *
 * Post:     "n" is modified.
 */
int isStrInt(char *str, int *n)
{
	// Provide a variable to detect at least one trailing non-whitespace
	// character following the numeric conversion.
	char c;

	// Convert the string to an integer and a character.
	int matches = sscanf(str, "%d %c", n, &c);

	// A string representation of an integer will result in only one match.
	return matches == 1;
}


/*
 * Function: listDir
 *
 * Purpose:  This function lists all files in the given directory.
 *
 * Receives: dirname  - c-string directory name
 *           numFiles - number of files in the given directory
 *
 * Returns:  The list of filenames is returned.
 *
 * Pre:      None
 *
 * Post:     'numFiles' is modified.
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


/*
 * Function: recvAll
 *
 * Purpose:  This function invokes 'recv' as many times as necessary to receive
 *           all of the given bytes of data.
 *
 * Receives: socket - connection endpoint on which to receive data
 *           buffer - data destination
 *           numBytes - target number of bytes to receive
 *
 * Returns:  None
 *
 * Pre:      A connection shall be established on the given socket.
 *
 *           'buffer' shall be large enough to hold the given number of bytes.
 *
 * Post:     'buffer' is modified.
 */
void recvAll(int socket, void *buffer, int numBytes)
{
	int ret;               // Return value for 'recv'
	int receivedBytes;     // Total number of bytes received

	// Retrieve the given number of bytes.
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


/*
 * Function: recvPacket
 *
 * Purpose:  This function receives a packet from the given socket.
 *
 *           The protocol is based on section 7.5 of Beej's Guide to
 *           Network Programming.
 *                              _   Header
 *             length - 2 bytes  |_/
 *             tag    - 8 bytes _|
 *             data   - X bytes
 *
 * Receives: socket - connection endpoint on which to receive data
 *           tag    - decorator for the encapsulated data
 *           data   - information buffer to transfer
 *
 * Returns:  None
 *
 * Pre:      A connection shall be established on the given socket.
 *
 *           Output buffers shall be large enough for their respective data.
 *
 * Post:     'tag' and 'data' are modified.
 */
void recvPacket(int socket, char *tag, char *data)
{
	unsigned short packetLength;       // Number of bytes in packet
	unsigned short dataLength;         // Number of bytes in encapsulated data
	char tmpTag[TAG_LEN + 1];          // Temporary tag transfer buffer
	char tmpData[MAX_PAYLOAD_LEN + 1]; // Temporary payload transfer buffer

	// Receive the packet length.
	recvAll(socket, &packetLength, sizeof(packetLength));
	packetLength = ntohs(packetLength);

	// Receive the tag field.
	recvAll(socket, tmpTag, TAG_LEN);
	tmpTag[TAG_LEN] = '\0';
	if (tag != NULL) { strcpy(tag, tmpTag); }

	// Receive the data.
	dataLength = packetLength - TAG_LEN - sizeof(packetLength);
	recvAll(socket, tmpData, dataLength);
	tmpData[dataLength] = '\0';
	if (data != NULL) { strcpy(data, tmpData); }
}


/*
 * Function: runControlSession
 *
 * Purpose:  This function communicates with a client over an FTP control
 *           connection.
 *
 * Receives: controlSocket - server-side endpoint of FTP control connection
 *           commandTag    - output buffer in which to store command tag
 *           dataPort      - output parameter in which to store data port
 *           filename      - output buffer in which to store filename
 *
 * Returns:  -1 on error, 0 otherwise
 *
 * Pre:      A connection shall be established on the given FTP control socket.
 *
 *           Output buffers shall be large enough for their respective data.
 *
 * Post:     'commandTag', 'dataPort', and 'filename' are modified.
 */
int runControlSession(int controlSocket, char *commandTag, int *dataPort, char* filename)
{
	char indata[MAX_PAYLOAD_LEN + 1];  // Input packet payload
	char intag[TAG_LEN + 1];           // Decorator for encapsulated input data
	char outdata[MAX_PAYLOAD_LEN + 1]; // Output packet payload
	char outtag[TAG_LEN + 1];          // Decorator for encapsulated output data

	// Receive data port from the client.
	printf("  Receiving data port (FTP active mode) ...\n");
	recvPacket(controlSocket, intag, indata);
	if (strcmp(intag, "DPORT") == 0) { *dataPort = atoi(indata); }

	// Receive command and filename from the client.
	printf("  Receiving command ...\n");
	recvPacket(controlSocket, intag, indata);
	strcpy(commandTag, intag);
	strcpy(filename, indata);

	// In the case of a malformed command, inform the client.
	if (strcmp(intag, "LIST") != 0 && strcmp(intag, "GET") != 0) {
		printf("  Transmitting command error ...\n");
		strcpy(outtag, "ERROR");
		strcpy(outdata, "Command must be either -l or -g");
		sendPacket(controlSocket, outtag, outdata);
		return -1;
	}

	// Otherwise, indicate that it is okay to establish an FTP data connection.
	else {
		printf("  Transmitting data-connection go-ahead ...\n");
		strcpy(outtag, "OKAY");
		sendPacket(controlSocket, outtag, "");
		return 0;
	}
}


/*
 * Function: runDataSession
 *
 * Purpose:  This function transfers file information over an FTP data
 *           connection.
 *
 * Receives: controlSocket - server-side endpoint of FTP control connection
 *           dataSocket    - server-side endpoint of FTP data connection
 *           commandTag    - c-string command identifier
 *           filename      - c-string name of requested file
 *
 * Returns:  -1 on error, 0 otherwise
 *
 * Pre:      Connections shall be established on the given FTP sockets.
 *
 * Post:     None
 */
int runDataSession(int controlSocket, int dataSocket, char *commandTag, char *filename)
{
	int ret = 0;     // Return value
	char **fileList; // List of filenames in the current directory
	int numFiles;    // Number of files the current directory
	int i;
	// Get a list of filenames in the current directory.
	fileList = listFiles(".", &numFiles);

	// The client requests transmission of filenames in the current directory.
	if (strcmp(commandTag, "LIST") == 0) {

		// Transfer each filename within a separate packet.
		printf("  Transmitting file listing ...\n");
		for (i = 0; i < numFiles; i++) {
			sendPacket(dataSocket, "FNAME", fileList[i]);
		}
	}

	// The client requests transmission of a file.
	else if (strcmp(commandTag, "GET") == 0) {
		do {
			char buffer[MAX_PAYLOAD_LEN + 1]; // File reader storage buffer
			int bytesRead;  // Number of bytes read from a file
			int fileExists; // Flag indicating if given filename exists in list
			FILE *infile;   // Reference to input file

			// Search the list of filenames in the current directory.
			fileExists = 0;
			for (i = 0; i < numFiles && !fileExists; i++) {
				if (strcmp(filename, fileList[i]) == 0) {
					fileExists = 1;
				}
			}

			// The given filename must exist.
			if (!fileExists) {
				printf("  Transmitting missing-file error ...\n");
				sendPacket(controlSocket, "ERROR", "File not found");
				ret = -1;
				break;
			}

			// Attempt to open the file.
			infile = fopen(filename, "r");
			if (infile == NULL) {
				printf("  Transmitting file-read-access error ...\n");
				sendPacket(controlSocket, "ERROR", "Unable to open file");
				ret = -1;
				break;
			}

			// Transfer the filename.
			sendPacket(dataSocket, "FILE", filename);

			// Transfer the file.
			printf("  Transmitting file ...\n");
			do {
				bytesRead = fread(buffer, sizeof(char), MAX_PAYLOAD_LEN, infile);
				buffer[bytesRead] = '\0';
				sendPacket(dataSocket, "FILE", buffer);
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

	// Tag the final packet to indicate that data transmission is complete.
	sendPacket(dataSocket, "DONE", "");

	// Inform the client that the control connection can be closed.
	printf("  Transmitting connection-termination go-ahead ...\n");
	sendPacket(controlSocket, "CLOSE", "");

	// Cleanup.
	for (i = 0; i < numFiles; i++) {
		free(fileList[i]);
	}
	free(fileList);

	return ret;
}


/*
 * Function: sendAll
 *
 * Purpose:  This function invokes 'send' as many times as necessary to send
 *           all of the given bytes of data.
 *
 * Receives: socket - connection endpoint on which to send data
 *           buffer - data source
 *           numBytes - target number of bytes to send
 *
 * Returns:  None
 *
 * Pre:      A connection shall be established on the given socket.
 *
 *           'buffer' shall be large enough to hold the given number of bytes.
 *
 * Post:     None
 */
void sendAll(int socket, void *buffer, int numBytes)
{
	int ret;           // Return value for 'send'
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


/*
 * Function: sendPacket
 *
 * Purpose:  This function sends a packet from the given socket.
 *
 *           The packet protocol is based on section 7.5 of Beej's Guide to
 *           Network Programming.
 *                              _   Header
 *             length - 2 bytes  |_/
 *             tag    - 8 bytes _|
 *             data   - X bytes
 *
 * Receives: socket - connection endpoint on which to send data
 *           tag    - decorator for the encapsulated data
 *           data   - information buffer to transfer
 *
 * Returns:  None
 *
 * Pre:      A connection shall be established on the given socket.
 *
 * Post:     None
 */
void sendPacket(int socket, char *tag, char *data)
{
	unsigned short packetLength;        // Number of bytes in packet
	char tagBuffer[TAG_LEN];            // Transmission buffer for given tag

	// Send the packet length.
	packetLength = htons(sizeof(packetLength) + TAG_LEN + strlen(data));
	sendAll(socket, &packetLength, sizeof(packetLength));

	// Send the tag field.
	memset(tagBuffer, '\0', TAG_LEN);   // Null-padding
	strcpy(tagBuffer, tag);
	sendAll(socket, tagBuffer, TAG_LEN);

	// Send the encapsulated data.
	sendAll(socket, data, strlen(data));
}


/*
 * Function: startFtpServer
 *
 * Purpose:  This function runs a server that listens on the given port and
 *           engages one client at a time over an FTP control connection.
 *
 * Receives: port - machine port to interface with clients
 *
 * Returns:  None
 *
 * Pre:      The listening port shall be at least 1024 to avoid conflicts with
 *           standard services.
 *
 *           The listening port shall be no greater than 65535 to stay within
 *           the bounds of available ports.
 *
 * Post:     Server persists until interrupted.
 */
void startFtpServer(int port)
{
	int serverSocket;                 // Socket for receiving client requests
	int status;                       // Return status
	struct sigaction interrupt;       // Signal action for handling interrupt
	struct sockaddr_in serverAddress; // Server address

	// Configure the server address.
	serverAddress.sin_family = AF_INET;         // IPv4
	serverAddress.sin_port = htons(port);       // Big-endian conversion
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Localhost

	// Create a server-side socket.
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1) {
		perror("socket");
		exit(1);
	}

	// Associate server-side socket with listening port.
	status = bind(serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress));
	if (status == -1) {
		perror("bind");
		exit(1);
	}

	// Listen for connections.
	status = listen(serverSocket, BACKLOG);
	if (status == -1) {
		perror("listen");
		exit(1);
	}

	// Register a callback to handle an interrupt signal.
	interrupt.sa_handler = &handleInterrupt;
	interrupt.sa_flags = 0;
	sigemptyset(&interrupt.sa_mask);
	status = sigaction(SIGINT, &interrupt, 0);
	if (status == -1) {
		perror("sigaction");
		exit(1);
	}

	// Provide FTP services to clients until interrupted.
	printf("ftserver: FTP server open on port %d\n", port);
	while (1) {
		char *clientIPv4;                   // Client dotted-decimal address
		char commandTag[TAG_LEN + 1];       // Buffer to store command tag
		char filename[MAX_PAYLOAD_LEN + 1]; // Buffer to store filename
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
		clientIPv4 = inet_ntoa(clientAddress.sin_addr);
		printf("\nftserver: FTP control connection established with \"%s\"\n", clientIPv4);

		// Communicate over FTP control connection.
		status = runControlSession(controlSocket, commandTag, &dataPort, filename);

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
			runDataSession(controlSocket, dataSocket, commandTag, filename);

			// Wait for client to acknowledge received data.
			recvPacket(controlSocket, NULL, NULL);

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
