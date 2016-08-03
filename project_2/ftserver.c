/*
* CS372: Project 1
* Chat Server: chatserve.py
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
