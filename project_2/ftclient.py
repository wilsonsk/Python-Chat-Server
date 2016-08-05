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

	sys.exit(0)	


# void checkArgs function: 
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



def checkPortArg(commandArg):
	#re.match(pattern, string, flags=0) -- attempts to match RE pattern to string with optional flags
	#returns a match object on success; None on failure
	#a regular expression is a special sequence of characters that helps you match or find other strings or sets of strings using a specialized syntax held in a pattern. RE are widely used in UNIX
	#this re checks if string parameter is a character 0-9 (i.e., is an integer)
	return re.match("^[0-9]+$", commandArg) is not None





main()
