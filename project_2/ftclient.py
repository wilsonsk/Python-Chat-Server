# CS372: Project 2
# FT Server: ftclient.py
# Skyler Wilson
# description:
        # Design and implement a simple FTP service between a server and a client
# requirements:
#       1. ftserver starts on host A and validates command-line parameters (<SERVER_PORT>)
#       2. ftserver waits on <PORTNUM> for a client request
#       3. ftclient starts on Host B, and validates any pertinent command-line parameters
#       (<SERVER_HOST>, <SERVER_PORT>, <COMMAND>, <FILENAME>, <DATA_PORT>, etc...)
#       4. ftserver and ftclient establist a TCP 'control' connection on <SERVER_PORT>. (call this connection C)
#       5. ftserver waits on connection P for ftclient to send command
#       6. ftclient sends command (-l (list) or -g <FILENAME> (get)) on connection C
#       7. ftserver receives command on connection C
#               * if ftclient sent an invalid command:
#                       * ftserver sends an error message to ftclient on connection C, and
#                       ftclient displays the message on-screen
#               * otherwise:
#                       * ftserver initiates a TCP 'data' connection with ftclient on
#                       <DATA_PORT> (call this connection D)
#                       * if ftclient has sent the -l command, ftserver sends its directory to
#                       ftclient on connection D, and ftclient displays the directory on-screen
#                       * if ftclient has sent -g <FILENAME> command, ftserver validates FILENAME
#                       and either:
#                               * sends the contents of FILENAME on connection D, ftclient saves
#                               the file in the current default directory (handling "duplicate file
#                               name" error if necessary) and displays a "transfer complete" message
#                               on-screen
#                       * or:
#                               * sends an appropriate error message ("File not found", etc.) to ftclient
#                               on connection C and ftclient displays the message on-screen
#                       * ftserver closes connection D (don't leave open sockets!)
#       8. ftclient closes connection C (don't leave open sockets!)
#       9. ftserver repeats from 2 (above) until terminated by a supervisor (SIGINT)
#
#       * ftclient is implemented in python
#       * ftserver is implemented in c
# references:
#     * see the included README.txt file for documentation of the program (chatclient.c)
#

import os	#operating systems library
import re	#regular expressions library
import sys	#systems library: use for command line arguments
from socket import(	#networking library: use for socket, etc.
	socket,
	gethostbyname,
	AF_INET,
	SOL_SOCKET,
	SO_REUSEADDR,
	SOCK_STREAM
)
from struct import pack, unpack		#performs conversions between Python values and C structs represented as Python strings
					# can be used in handling binary data stored in files or from network connections


#The backlog argument specifies the maximum number of queued connections and should be at least 0; 
#the maximum value is system-dependent (usually 5), the minimum value is forced to 0
BACKLOG = 5
ARG_LEN = 8 	#number of bytes for <COMMAND> option argument

def main():
	global dataConnPort
	global filename
	global commandArg
	global s_port
	global s_host

	checkArgs()

	print "s_host = " + str(s_host)
	
	ftp()

	sys.exit(0)	


# checkArgs function: 
#	* inputs:
#		* the global command line arguments defined at top of main()
#	* outputs:
#		* on success -- void
#		* on failure -- exit(1)
#	* calls: 
#		* checkPortArg(char* portArg, int* s_portno)
#	* purpose:
#		* checks the number of command line arguments (5 or 6)
#		* checks command option (-l | -g)
#		* initialize global command line arguments
#		* checks that argument 1 <SERVER_PORT> is an integer 
#		* checks that argument 1 <SERVER_PORT> is within range
#		* check the command line arugment assigned values
#
def checkArgs():
	if(len(sys.argv) not in (5, 6)):
		print "usage: python ftclient <SERVER-HOSTNAME> <SERVER_PORT> -l | -g [<FILENAME>] <DATA_PORT>"
		sys.exit(1)

	s_host = gethostbyname(sys.argv[1])
	s_port = sys.argv[2]
	commandArg = sys.argv[3]
	filename = sys.argv[4] if len(sys.argv) == 6 else None
	dataConnPort = sys.argv[5] if len(sys.argv) == 6 else sys.argv[4]

	print "s_host = " + str(s_host)
	print "s_port = " + str(s_port)
	print "commandArg = " + str(commandArg)
	print "filename = " + str(filename)
	print "dataConnPort = " + str(dataConnPort)

	#check -g option associated with proceeding filename
	if commandArg == "-g" and filename is None:
		print "usage: python ftclient <SERVER-HOSTNAME> <SERVER_PORT> -l | -g [<FILENAME>] <DATA_PORT>"
		sys.exit(1)

	#check if s_port is an integer
	if not checkPortArg(s_port):
		print "error: <SERVER_PORT> argument must be an integer"
		sys.exit(1)
	s_port = int(s_port)

	#check range of s_port 1024 - 65535
	if int(s_port) < 1024 or int(s_port) > 65535:
		print "error: <SERVER_PORT> argument must be in range [1024, 65535]"
		sys.exit(1)

	if(s_port == dataConnPort):
		print "error: Server port can't be same as data port"
		sys.exit(1)

# checkPortArg function
#	* inputs:
#		* commandArg -- uses's <COMMAND> option
#	* outputs:
#		* on success -- a match object 
#		* on failure -- None
#	* calls:
#		* re.match()
#	* purpose:
#		* attempts to match regular expression pattern to a string
#
def checkPortArg(commandArg):
	#re.match(pattern, string, flags=0) -- attempts to match RE pattern to string with optional flags
	#returns a match object on success; None on failure
	#a regular expression is a special sequence of characters that helps you match or find other strings or sets of strings using a specialized syntax held in a pattern. RE are widely used in UNIX
	#this re checks if string parameter is a character 0-9 (i.e., is an integer)
	return re.match("^[0-9]+$", commandArg) is not None

# ftp function
#	* inputs:
#		* global variables
#	* outputs
#		* on success -- none
#		* on failure -- exit(1) 
#	* calls:
#		* controlConnection(int sockfd)
#		* bind()
#		* list()
#		* format()
#		* accept()
#		* setsockopt()
#		* socket()
#		* connect()
#		* close()
#	* purpose:
#		* file transfer service between client and server
#
def ftp():

	print "s_host = " + str(s_host)
	print "s_port = " + str(s_port)
	print "commandArg = " + str(commandArg)
	print "filename = " + str(filename)
	print "dataConnPort = " + str(dataConnPort)
	try:
		control_sockfd = socket(AF_INET, SOCK_STREAM, 0)
	except Exception as x:
		print x.strerror
		sys.exit(1)

	try:
		control_sockfd.connect((s_host, s_port))
	except Exception as x:
		print x.strerror
		sys.exit(1)	

	print "control connection established with " + {0}.format(s_host, s_port)	
	status = controlConnection(control_sockfd)
	
	if status != -1:
		try:
			client_sockfd = socket(AF_INET, SOCK_STREAM, 0)
		except Exception as x:
			print x.strerror
			sys.exit(1)

		#bind client socket with specified data port
		try:
			client_sockfd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
			client_sockfd.bind(("", dataConnPort))
		except Exception as x:
			print x.strerror
			sys.exit(1)

		# listen for connections
		try:
			client_sockfd.listen(BACKLOG)
		except Exception as x:
			print x.strerror
			sys.exit(1)
	
		#FTP data connection
		try:
			data_sockfd = client_sockfd.accept()[0]
		except Exception as x:
			print x.strerror
			sys.exit(1)
		print "FTP data connection with: " + {0}.format(s_host)
		
		#FTP
		dataConnection(control_sockfd, data_sockfd)

		#error feedback
		while true:
			optionBuf, dataBuf = recvPack(control_sockfd)
			if optionBuf == "ERROR":
				print "recv optionBuf" + optionBuf
			if dataBuf == "CLOSE":
				break

		#close control connection
		try:
			control_sockfd.close()
		except Exception as x:
			print x.strerror
			sys.exit(1)
		print "FTP control connection closed"	

# controlConnection function
#	* inputs:
#		* control_sockfd -- socket file descriptor of control connection
#	* outputs:
#		* on success -- 0
#		* on failure -- -1
#	* calls:
#		* str()
#		* recvPack()
#		* sendPack()
#	* purpose:
#		* creates control connection 
#
def controlConnection(control_sockfd):
	optionBuf = "DPORT"
	dataBuf = str(dataConnPort)
	sendPack(control_sockfd, optionBuf, dataBuf)
	
	optionBuf = "NULL"
	dataBuf = ""
	
	if(commandArg == "-l"):
		optionBuf = "LIST"
	elif(commandArg == "-g"):
		optionBuf = "GET"
		dataBuf = filename

	sendPack(control_sockfd, optionBuf, dataBuf)
	
	#read in server response
	servOptionBuf, servDataBuf = recvPack(control_sockfd)	
	
	#error check
	if(servOptionBuf == "ERROR"):
		return -1

	return 0

# dataConnection function
#	* inputs:
#		* control_sockfd -- control connection socket
#		* data_sockfd -- data connection socket
#	* outputs:
#		* on success -- 0
#		* on failure -- -1
#	* calls:
#		* os.path.exists()
#		* open()
#		* recvPack()
#		* write()
#		* sendPack()
#	*purpose:
#		* creates data connection 
def dataConnection(control_sockfd, data_socketfd):
	check = 0

	optionBuf, dataBuf = recvPack(data_sockfd)

	#print list of file names
	if(option == "FNAME"):
		while(optionBuf != "DONE"):
			print dataBuf + "\n"
			optionBuf, dataBuf = recvPack(data_sockfd)
			
	elif(optionBuf == "FILE"):
		filename = dataBuf
		if(os.path.exists(filename)):
			print "file: {0} already exists".format(filename)
			check = -1
		else:
			#write new file from data
			with open(filename, "W") as newfile:
				while(optionBuf != "DONE"):
					optionBuf, dataBuf = recvPack(data_sockfd)
					newfile.write(dataBuf)
			print "FTP complete"

	else:
		check = -1

		sendPack(control_sockfd, "ACK", "")

		return check


# recvPack function
#	* inputs:
#		* sockfd -- socket to receive packet from
#	* outputs:
#		* pair of received <COMMAND> option and data
#	* calls:
#		* recvFile()
#		* unpack(fmt, string) -- unpack the string according to the given 
#		format.
#				      -- returns a tuple even if it contains 1 item
#				      -- the string must contain exactly the amount
#					of data required by the format
#		* rstrip([chars]) -- returns a copy of the string in which all chars
#		have been stripped from the end of the string 
#				  -- returns a copy of the string in which all chars
#				have been stripped from end of string
#	* purpose:
#		* receive packet from the specified socket
#
def recvPack(sockfd):
	packLen = unpack(">H", recvFile(sockfd, 2))[0]
	
	#read in <COMMAND> option
	#stip termination string character off string 
	option = recvAll(sockfd, ARG_LEN).rstrip("\0")
	
	data = recvFile(sockfd, packLen - ARG_LEN - 2)
	
	return option, data

# recvFile function
#	* inputs:
#		* sockfd -- file descriptor of socket to receive from
#		* presetSize -- specified number of bytes to receive from
#	* outputs:
#		* data (file) received stored in dataBuffer
#	* calls:
#		* len()
#		* recv()
#	* purpose:
#		* receive specified number of bytes from data (file)
#
def recvFile(sockfd, presetSize):
	data = ""
	while(len(data) < presetSize):
		try:
			data += sockfd.recv(presetSize - len(data))
		except Exception as x:
			print x.strerror
			sys.exit(1)

	return data


# sendPack function
#	* inputs:
#		* sockfd -- socket file descriptor to send from 
#		* option -- send option 
#		* data -- send data
#	* outputs:
#		* None
#	* calls:
#		* ljust(width[, fillchar]) -- returns the string left justified in a 
#		string of length
#			* width -- string length in total after padding
#			* fillchar -- the filler character, default is space
#		width. Padding is done using the specified fillchar (default is a 
#		space). The original string is returned if width is less than len(s)
#		* len()
#		* pack(fmt, v1, v2, ...) -- returns a string containing the value 
#		v1, v2, .. packed according to the given format. The arguments must
#		match the values required by the format exaclty  
#		* sendall()
#	* purpose:
#		* send packet from specified socket
#
def sendPack(sockfd, option = "", data = ""):
	packLen = 2 + ARG_LEN + len(data)
	packet = pack(">H", packLen)
	packet += option.ljust(ARG_LEN, "\0")
	packet += data

	#send to server
	try:
		sockfd.sendall(packet)
	except Exception as x:
		print x.strerror
		sys.exit(1)

#run main
main()
