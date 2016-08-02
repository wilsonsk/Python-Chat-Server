#
# CS372: Project 1
# Chat Server: chatserve.py
# Skyler Wilson
# description:
        # Design and implement a simple chat system that works for a pair of users (client, server)
# requirements:
#       1. chatserve starts on host A
#       2. chatserve on host A waits on a port (specified on command line) for a client requrest
#       3. chatclient starts on host B specifying host A's hostname and port number on command line
#       4. chatclient on host B gets the user's "handle" by initial query (one word name, up to 10 chars)
#               * chatclient will display this handle as a prompt on host B
#               * chatclient will prepend handle to all messages sent to host A (e.g., "SteveO> Hi!!!")
#       5. chatclient on host B sends an initial message to chatserve on host A : PORTNUM
#               * This causes a connection to be established between host A and host B
#               * host A and host B are now peers and may alternate sending/receiving messages
#               * responses from host A should have host A's "handle" prepended
#       6. host A responds to host B or closes the connection with the command "quit"
#       7. host B responds to host A or closes the connection with the command "quit"
#       8. if the connection is not closed, repeat from 6
#       9. if the connection is closed, chatserve repeats from 2 (until SIGINT is received)
#       * chatserve is implemented in python
#       * chatclient is implemented in c
# references:
#       * see the included README.txt file for documentation of the program (chatclient.c)


import socket
import sys
import os

def chat(client_sock):
	quit = False
	print "waiting for response from client...\n"
	while(quit == False):
		# socket.recv(bufsize[flags])
		# Receive data from the socket. The return value is a string representing the data received. The maximum amount of data to be received at once is specified by bufsize.
		# See the Unix manual page recv(2) for the meaning of the optional argument flags; it defaults to zero
		rcvdMessage = client_sock.recv(525)
		if(rcvdMessage == 0):
			print "error with receiving message from client\n"
			break
		if(rcvdMessage == "quit"):
			print 'connection closed by client\n'
			quit = True
			client_sock.close()
			break			

		print rcvdMessage

		valid_in = False
		while(valid_in != True):
			sentMessage = raw_input("Enter a message or enter command 'quit' to shutdown this server: ")
			if(len(sentMessage) < 512):
				valid_in = True
		if(sentMessage == "quit"):
			print 'connection closed by server'
			client_sock.send(sentMessage)
			quit = True
			# socket.shutdown(SHUT_RDWR)
			client_sock.close()
			return -1;
		else:
			client_sock.send(sentMessage)
			print "waiting for response from client...\n"

def main():
	# Create a new socket using the given address family, socket type and protocol number. The address family should be AF_INET (the default), AF_INET6 or AF_UNIX. 
	# The socket type should be SOCK_STREAM (the default), SOCK_DGRAM or perhaps one of the other SOCK_ constants. The protocol number is usually zero and may be omitted in that case.
	server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
	# Running an example several times with too small delay between executions, could lead to this error:
	# socket.error: [Errno 98] Address already in use
	# There is a socket flag to set, in order to prevent this, socket.SO_REUSEADDR:
	server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	# the SO_REUSEADDR flag tells the kernel to reuse a local socket in TIME_WAIT state, without waiting for its natural timeout to expire.

	# Then bind() is used to associate the socket with the server address
	portno = int(sys.argv[2])
	server_address = (socket.gethostbyname(sys.argv[1]), portno)
	# serve_addr_str = str(server_address)
	server_sock.bind(server_address)

	# listen for incoming connections
	# listen() puts the socket into server mode
	# Listen for connections made to the socket. The backlog argument specifies the maximum number of queued connections and should be at least 0; 
	# the maximum value is system-dependent (usually 5), the minimum value is forced to 0
	server_sock.listen(1)
	print "Listening for connections on server socket: " + str(server_address)
	
	server_open = True
	while(server_open == True):
		# accept() waits for an incoming connection
		# Accept a connection. The socket must be bound to an address and listening for connections. The return value is a pair (conn, address) where conn is a new socket 
		# object usable to send and receive data on the connection, and address is the address bound to the socket on the other end of the connection
		client_sock, client_addr = server_sock.accept()
		print "connection made with remote socket: " + str(client_addr)
		
		n = chat(client_sock)
		if(n == -1):
			server_open = False
		else:
			print "Listening for connections on server socket: " + str(server_address)
	server_sock.close()
main()
