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

import os                       # Miscellaneous operating system interfaces
import re                       # Regular expressions
import sys                      # System-specific parameters
from socket import (            # Sockets API
    socket,
    gethostbyname,
    AF_INET,
    SOCK_STREAM,
    SOL_SOCKET,
    SO_REUSEADDR
)
from struct import pack, unpack # Structured binary data

BACKLOG = 5 # Arbitrary queue size for connection requests
TAG_LEN = 8 # Number of bytes reserved for tag field of packet header

def main():
    # Provide global access to commandArg-line arguments.
    global s_host
    global s_port
    global commandArg
    global filename
    global dataPort

    # Exactly five or six commandArg-line arguments are expected.
    if len(sys.argv) not in (5, 6):
        print (
            "usage: python2 ftclient <server-hostname> <server-port> " +
            "-l|-g [<filename>] <data-port>"
        )
        sys.exit(1)
    s_host = gethostbyname(sys.argv[1])
    s_port = sys.argv[2]
    commandArg = sys.argv[3]
    filename = sys.argv[4] if len(sys.argv) == 6 else None
    dataPort = sys.argv[5] if len(sys.argv) == 6 else sys.argv[4]

    # The -g (get) commandArg must by accompanied by a filename.
    if commandArg == "-g" and filename is None:
        print (
            "usage: python2 ftclient <server-hostname> <server-port> " +
            "-l|-g [<filename>] <data-port>"
        )
        sys.exit(1)

    # The given server port must be an integer.
    if not checkPortArg(s_port):
        print "ftclient: Server port must be an integer"
        sys.exit(1)
    s_port = int(s_port)

    # The given server port must be in the range [1024, 65535].
    if int(s_port) < 1024 or int(s_port) > 65535:
        print "ftclient: Server port must be in the range [1024, 65535]"
        sys.exit(1)

    # The given commandArg must be either -l (list) or -g (get).
    if commandArg not in ("-l", "-g"):
        print "ftclient: Command must be either -l or -g"
        sys.exit(1)

    # The given data port must be an integer.
    if not checkPortArg(dataPort):
        print "ftclient: Data port must be an integer"
        sys.exit(1)
    dataPort = int(dataPort)

    # The given data port must be in the range [1024, 65535].
    if int(dataPort) < 1024 or int(dataPort) > 65535:
        print "ftclient: Data port must be in the range [1024, 65535]"
        sys.exit(1)

    # The given server and data ports must be distinct from eachother.
    if s_port == dataPort:
        print "ftclient: Server port and data port cannot match"
        sys.exit(1)

    # Establish a control connection between the FTP client and server.
    ftp()

    sys.exit(0)


# checkPortArg function
#	* inputs:
#		* commandArgArg -- uses's <COMMAND> option
#	* outputs:
#		* on success -- a match object 
#		* on failure -- None
#	* calls:
#		* re.match()
#	* purpose:
#		* attempts to match regular expression pattern to a string
#
def checkPortArg(string):
    # Attempt to match an integer substring.
    return re.match("^[0-9]+$", string) is not None


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
def recvFile(socket, numBytes):
    # Retrieve the given number of bytes.
    data = "";
    while len(data) < numBytes:
        try:
            data += socket.recv(numBytes - len(data))
        except Exception as e:
            print e.strerror
            sys.exit(1);

    return data


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

def recvPack(socket):
    # Receive the packet length.
    packetLength = unpack(">H", recvFile(socket, 2))[0]

    # Receive the tag field.
    tag = recvFile(socket, TAG_LEN).rstrip("\0")

    # Receive the encapsulated data.
    data = recvFile(socket, packetLength - TAG_LEN - 2)

    return tag, data


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
def controlConnection(controlSocket):
    # Send given data port to the server.
    print "  Transmitting data port (FTP active mode) ..."
    outtag = "DPORT"
    outdata = str(dataPort)
    sendPack(controlSocket, outtag, outdata)

    # Send given commandArg to the server.
    print "  Transmitting commandArg ..."
    outtag = "NULL"
    outdata = ""
    if commandArg == "-l":
        outtag = "LIST"
    elif commandArg == "-g":
        outtag = "GET"
        outdata = filename
    sendPack(controlSocket, outtag, outdata)

    # Receive the server's response.
    intag, indata = recvPack(controlSocket)

    # In the case of a server-side error, provide feedback for the user.
    if intag == "ERROR":
        print "ftclient: " + indata
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
def dataConnection(controlSocket, dataSocket):
    ret = 0 # Return value

    # Retrieve the first packet from the server.
    intag, indata = recvPack(dataSocket)

    # A list of filenames is being transferred.
    if intag == "FNAME":
        print "ftclient: File listing on \"{0}\"".format(s_host, s_port)

        # Print all received filenames.
        while intag != "DONE":
            print "  " + indata
            intag, indata = recvPack(dataSocket)

    # A file is being transferred.
    elif intag == "FILE":
        # Don't allow files to be overwritten.
        filename = indata
        if os.path.exists(filename):
           print "ftclient: File \"{0}\" already exists".format(filename)
           ret = -1

        # Write the received data to file.
        else:
            with open(filename, "w") as outfile:
                while intag != "DONE":
                    intag, indata = recvPack(dataSocket)
                    outfile.write(indata)
            print "ftclient: File transfer complete"

    # An error occurred.
    else:
        ret = -1

    # Acknowledge receipt of all packets.
    sendPack(controlSocket, "ACK", "")

    return ret


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
def sendPack(socket, tag = "", data = ""):
    # Determine packet length.
    packetLength = 2 + TAG_LEN + len(data)

    # Build packet.
    packet = pack(">H", packetLength)
    packet += tag.ljust(TAG_LEN, "\0")
    packet += data

    # Send packet to the server.
    try:
        socket.sendall(packet)
    except Exception as e:
        print e.strerror
        sys.exit(1)


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
    # Create client-side endpoint of FTP control connection.
    try:
        controlSocket = socket(AF_INET, SOCK_STREAM, 0)
    except Exception as e:
        print e.strerror
        sys.exit(1)

    # Establish FTP control connection.
    try:
        controlSocket.connect((s_host, s_port))
    except Exception as e:
        print e.strerror
        sys.exit(1)
    print ("ftclient: FTP control connection established with " +
           "\"{0}\"".format(s_host, s_port)          )

    # Communicate over the control connection.
    status = controlConnection(controlSocket)

    # Accept FTP data services if control session was successful.
    if status != -1:
        # Create client-side socket.
        try:
            clientSocket = socket(AF_INET, SOCK_STREAM, 0)
        except Exception as e:
            print e.strerror
            sys.exit(1)

        # Associate client-side socket with given data port.
        try:
            clientSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
            clientSocket.bind(("", dataPort))
        except Exception as e:
            print e.strerror
            sys.exit(1)

        # Listen for connections.
        try:
            clientSocket.listen(BACKLOG)
        except Exception as e:
            print e.strerror
            sys.exit(1)

        # Establish FTP data connection.
        try:
            dataSocket = clientSocket.accept()[0]
        except Exception as e:
            print e.strerror
            sys.exit(1)
        print ("ftclient: FTP data connection established with " +
               "\"{0}\"".format(s_host)                       )

        # Transfer file information over FTP data connection.
        dataConnection(controlSocket, dataSocket)

        # Display all queued error messages sent along control connection.
        while True:
            intag, indata = recvPack(controlSocket)
            if intag == "ERROR":
                print "ftclient: " + indata
            if intag == "CLOSE":
                break

    # Close FTP control connection.
    try:
        controlSocket.close()
    except Exception as e:
        print e.strerror
        sys.exit(1)
    print "ftclient: FTP control connection closed"

main()
