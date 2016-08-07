# Brett Fedack
# CS372 Summer 2015
# Project 1
# Description: This program is a client that connects with a companion FTP
#   server program. Once connected, a client can request either a file listing
#   of the server's current directory or one-way transmission of a file from
#   server to client. The FTP session is managed over a control connection, and
#   the transmission of file information occurs over a separate data connection
#   in a manner that is consistent with FTP active mode.

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
    # Provide global access to command-line arguments.
    global serverHost
    global serverPort
    global command
    global filename
    global dataPort

    # Exactly five or six command-line arguments are expected.
    if len(sys.argv) not in (5, 6):
        print (
            "usage: python2 ftclient <server-hostname> <server-port> " +
            "-l|-g [<filename>] <data-port>"
        )
        sys.exit(1)
    serverHost = gethostbyname(sys.argv[1])
    serverPort = sys.argv[2]
    command = sys.argv[3]
    filename = sys.argv[4] if len(sys.argv) == 6 else None
    dataPort = sys.argv[5] if len(sys.argv) == 6 else sys.argv[4]

    # The -g (get) command must by accompanied by a filename.
    if command == "-g" and filename is None:
        print (
            "usage: python2 ftclient <server-hostname> <server-port> " +
            "-l|-g [<filename>] <data-port>"
        )
        sys.exit(1)

    # The given server port must be an integer.
    if not isStrInt(serverPort):
        print "ftclient: Server port must be an integer"
        sys.exit(1)
    serverPort = int(serverPort)

    # The given server port must be in the range [1024, 65535].
    if int(serverPort) < 1024 or int(serverPort) > 65535:
        print "ftclient: Server port must be in the range [1024, 65535]"
        sys.exit(1)

    # The given command must be either -l (list) or -g (get).
    if command not in ("-l", "-g"):
        print "ftclient: Command must be either -l or -g"
        sys.exit(1)

    # The given data port must be an integer.
    if not isStrInt(dataPort):
        print "ftclient: Data port must be an integer"
        sys.exit(1)
    dataPort = int(dataPort)

    # The given data port must be in the range [1024, 65535].
    if int(dataPort) < 1024 or int(dataPort) > 65535:
        print "ftclient: Data port must be in the range [1024, 65535]"
        sys.exit(1)

    # The given server and data ports must be distinct from eachother.
    if serverPort == dataPort:
        print "ftclient: Server port and data port cannot match"
        sys.exit(1)

    # Establish a control connection between the FTP client and server.
    startFtpClient()

    sys.exit(0)


# Function: isStrInt
#
# Purpose:  This function determines if the given string represents an integer.
#
# Receives: string - string that may represent an integer
#
# Returns:  Whether or not the string represents an integer is returned.
#
# Pre:      None
#
# Post:     None
def isStrInt(string):
    # Attempt to match an integer substring.
    return re.match("^[0-9]+$", string) is not None


# Function: recvAll
#
# Purpose:  This function invokes "recv" as many times as necessary to receive
#           all of the given bytes of data.
#
# Receives: socket   - connection endpoint on which to receive data
#           numBytes - target number of bytes to receive
#
# Returns:  The buffer of received data is returned.
#
# Pre:      A connection shall be established on the given socket.
#
# Post:     None
def recvAll(socket, numBytes):
    # Retrieve the given number of bytes.
    data = "";
    while len(data) < numBytes:
        try:
            data += socket.recv(numBytes - len(data))
        except Exception as e:
            print e.strerror
            sys.exit(1);

    return data


# Function: recvPacket
#
# Purpose:  This function receives a packet from the given socket.
#
#           The packet protocol is based on section 7.5 of Beej's Guide to
#           Network Programming.
#                              _   Header
#             length - 2 bytes  |_/
#             tag    - 8 bytes _|
#             data   - X bytes
#
# Receives: socket - connection endpoint on which to receive data
#
# Returns:  A (tag, data) tuple is returned.
#
# Pre:      A connection shall be established on the given socket.
#
# Post:     None
def recvPacket(socket):
    # Receive the packet length.
    packetLength = unpack(">H", recvAll(socket, 2))[0]

    # Receive the tag field.
    tag = recvAll(socket, TAG_LEN).rstrip("\0")

    # Receive the encapsulated data.
    data = recvAll(socket, packetLength - TAG_LEN - 2)

    return tag, data


# Function: runControlSession
#
# Purpose:  This function communicates with a server over an FTP control
#           connection.
#
# Receives: controlSocket - client-side endpoint of FTP control connection
#
# Returns:  -1 on error, 0 otherwise
#
# Pre:      A connection shall be established on the given FTP control socket.
#
#           Globals shall be defined for "command", "dataPort", and "filename".
#
#           The data port shall be at least 1024 to avoid conflicts with
#           standard services.
#
#           The data port shall be no greater than 65535 to stay within the
#           bounds of available ports.
#
# Post:     None
def runControlSession(controlSocket):
    # Send given data port to the server.
    print "  Transmitting data port (FTP active mode) ..."
    outtag = "DPORT"
    outdata = str(dataPort)
    sendPacket(controlSocket, outtag, outdata)

    # Send given command to the server.
    print "  Transmitting command ..."
    outtag = "NULL"
    outdata = ""
    if command == "-l":
        outtag = "LIST"
    elif command == "-g":
        outtag = "GET"
        outdata = filename
    sendPacket(controlSocket, outtag, outdata)

    # Receive the server's response.
    intag, indata = recvPacket(controlSocket)

    # In the case of a server-side error, provide feedback for the user.
    if intag == "ERROR":
        print "ftclient: " + indata
        return -1
    return 0


# Function: runDataSession
#
# Purpose:  This function transfers file information over an FTP data
#           connection.
#
# Receives: controlSocket - client-side endpoint of FTP control connection
#           dataSocket    - client-side endpoint of FTP data connection
#
# Returns:  -1 on error, 0 otherwise
#
# Pre:      Connections shall be established on the given FTP sockets.
#
# Post:     The file contents of the current working directory are modified.
def runDataSession(controlSocket, dataSocket):
    ret = 0 # Return value

    # Retrieve the first packet from the server.
    intag, indata = recvPacket(dataSocket)

    # A list of filenames is being transferred.
    if intag == "FNAME":
        print "ftclient: File listing on \"{0}\"".format(serverHost, serverPort)

        # Print all received filenames.
        while intag != "DONE":
            print "  " + indata
            intag, indata = recvPacket(dataSocket)

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
                    intag, indata = recvPacket(dataSocket)
                    outfile.write(indata)
            print "ftclient: File transfer complete"

    # An error occurred.
    else:
        ret = -1

    # Acknowledge receipt of all packets.
    sendPacket(controlSocket, "ACK", "")

    return ret


# Function: sendPacket
#
# Purpose:  This function sends a packet from the given socket.
#
#           The packet protocol is based on section 7.5 of Beej's Guide to
#           Network Programming.
#                              _   Header
#             length - 2 bytes  |_/
#             tag    - 8 bytes _|
#             data   - X bytes
#
# Receives: socket - connection endpoint on which to send data
#           tag    - decorator for the encapsulated data
#           data   - information buffer to transfer
#
# Returns:  None
#
# Pre:      A connection shall be established on the given socket.
#
# Post:     None
def sendPacket(socket, tag = "", data = ""):
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


# Function: startFtpClient
#
# Purpose:  This function establishes an FTP control connection between the
#           client and given server.
#
# Receives: None
#
# Returns:  None
#
# Pre:      Globals shall be defined for "serverHost" and "serverPort".
#
#           The server-side port shall be at least 1024 to avoid conflicts with
#           standard services.
#
#           The server-side port shall be no greater than 65535 to stay within
#           the bounds of available ports.
#
# Post:     None
def startFtpClient():
    # Create client-side endpoint of FTP control connection.
    try:
        controlSocket = socket(AF_INET, SOCK_STREAM, 0)
    except Exception as e:
        print e.strerror
        sys.exit(1)

    # Establish FTP control connection.
    try:
        controlSocket.connect((serverHost, serverPort))
    except Exception as e:
        print e.strerror
        sys.exit(1)
    print ("ftclient: FTP control connection established with " +
           "\"{0}\"".format(serverHost, serverPort)          )

    # Communicate over the control connection.
    status = runControlSession(controlSocket)

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
               "\"{0}\"".format(serverHost)                       )

        # Transfer file information over FTP data connection.
        runDataSession(controlSocket, dataSocket)

        # Display all queued error messages sent along control connection.
        while True:
            intag, indata = recvPacket(controlSocket)
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


# Define script point of entry.
if __name__ == "__main__":
	main()
