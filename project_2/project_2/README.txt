/*
* CS372: Project 2
* Project 2: README.txt
* Skyler Wilson
*/

* Unzip, Compile, Execute, and Control project_1: Instructions:
	* unzip project_1.zip:
		1. enter the directory containing 'project_2.zip'
		2. enter command:
			unzip project_2.zip
		3. contents of this zip folder will now be within your current working directory for access:
			* ftclient.py ftserver.c makefile README.txt
	* Compile ftserver.c:
		1. enter the directory containing the file 'ftserver.c'
		2. enter the command:
			make
		3. this command will compile the ftserver.c program by default 
	* Executing ftserver and ftclient:
		1. enter the directory containing project_2 files
		2. run the server first!
			* enter the command:
				./ftserver <PORTNO>
			* enter a port number 1024 < portno < 65535
		3. open a separate terminal
		4. enter the directory containing the project_2 files
		5. run the client second!
			* enter the command:
				python ftclient.py localhost <samePortNoAsServer> <-l || -g> <FILENAME if -g> <DATAPORT> 
			* enter the same port number used to run the server (i.e., the 3rd argument when running the chatserve.py on command line)
			* data port must be 1024 < DATAPORT < 65535
	* Control Flow of FTP service:
		1. user runs server
		2. user opens new terminal
		3. user runs client
		4. server will be opened and listening for any connections made to its socket
		5. client will connect to server socket
		6. server will accept this new connection to the clien
		7. server will receive and display client's command (either list files or deliver a specified file within that server's directory
		8. client connection will end, server will remain open
		9. server can be shut down via Ctrl-c

* Basic Server Socket Architecture:
	 
///Procedure:
        //Create a network endpoint with socket()
        //Bind socket to a port with bind()
        //Start listening for connections with listen()
        //Loop and accept connections with accept()
        //Read and write data to a client with send(), recv(), read(), and write(//Procedure:
        //Create a network endpoint with socket()
        //Bind socket to a port with bind()
        //Start listening for connections with listen()
        //Loop and accept connections with accept()
        //Read and write data to a client with send(), recv(), read(), and write())* A simple server in the internet domain using TCP
//The port number is passed as an argument
//socket: endpoint of communication between 2 processes; 2-Tuple composed of IP and port number
//connection: 4-Tuple composed of 2 sets of IP and port numbers
//Processes: running instance of a program; the only things in UNIX that are NOT files
//Files: stream of bytes; persistent; usually stored on magnetic disk
//Thread: a running instance of a program within a program

//Procedure:
	//Create a network endpoint with socket()
	//Bind socket to a port with bind()
	//Start listening for connections with listen()
	//Loop and accept connections with accept()
	//Read and write data to a client with send(), recv(), read(), and write()
	
//Ports allow multiple network processes on a machine with a single address
	//server has to choose a port where clients can contact it


//bind()
	// int bind(int sockfd, struct sockaddr* address,  size_t add_len);
		//2nd parameter: network address structure identifying port to listen on
			//accept any addr
		//returns 0 success, -1 on error

//All connections to server are ignored until you tell the socket() to start listening
//listen()
	//int listen(int sockfd, int queue_size);
		//queue_size: max number of connections to queue up
		//retunrs 0 on success, -1 on error

//accept(): takes the next connection off of the listen queue or blocks the process until a connection arrives
	//int accept(int sockfd, struct sockaddr* address, size_t &add_len);
		//address: network address of identifying client. This can be a NULL pointer
		//add_len: a pointer to a variable containing the size of the address structure, can be NULL
		//returns a file descriptor or -1
			//ex:
				//while(1){
					client_sockfd = accept(sockfd, NULL, NULL);
					if(client_sockfd == -1){
						perror("accept call failed");
					}
				}

//select(): is designed for server-like applications that have many communication channels open at once
		//data or space may become available at any time on any of the channels
		//you want to minimize the delay between when data/space becomes available and your process takes action (calls read() or write())
	//you give select a list of file descriptors and select() returns when any one of those selectors becomes readable or writeable
	//returns -1 if error, or 0 if time out(no event occurred), or the number of interesting fds (fds where something happened)

	//int select(
//		int nfds,		// number of fds of interest
//		fd_set* readfds,	// input fds of interest
//		fd_set* writefds,	// output fds of interest
//		fd_set* errorfds,	// fds where exception has ocurred
//		struct timeval* timeout	// when to timeout
//	)

		//nfds: should be larger than the number of fds interested in: to prevent passing a larger number than nfds which prevents access
		//    : FD_SETSIZE: max number of fds
		//    : probably more efficient if you pass the exact number of fds you are interested in

		//fd_sets: all 3 parameters are bit masks: 
			//each bit of the number refers to one fd
			// bit 0 -> fd0, bit 1 -> fd1, etc.
		//UNIX provides you with functions to manipulate bit masks for the select() system call
		//	FD_ZERO - sets all bits to 0
		//	FD_SET - set one specific bit to 1
		// 	FD_ISSET - determine if a specific but is set to 1
		// 	FD_CLEAR - set one specific but to 0

			//example:
//		int fd1, fd2;
//		fd_set readset;
//		fd1 = open("file1", O_RDONLY);
//		fd2 = open("file2", O_RDONLY);

//		FD_ZERO(&readset);		//zero out readset: initialize
//		FD_SET(fd1, &readset);		//indicate in readset that these 2 fds which are ints, are fdss that we interested in
//		FD_SET(fd2, &readset);		//when either of these 2 fds become readable, the bit is set in the readfds
			//if fd became readable, the bit is set int he readfds based on FD_SET
			//if a fd becames writeable, the bit is set in the sritefds
			//if an error occurred, the bit is set in the errorfds
	
//		switch(select(2, &readset, NULL, NULL, NULL));

	//TO FIND WHERE THE EVENT OCCURRED: You need to loop through your fds of interest to determine which ones the event occurred on
//		for(i = 0; i < 2; i++){
//			if(FD_ISSET(i, &readset)){
//				//if there's data to read on fd ... read(i, ...);
//			}
//		}

//Iterative: involving repitition
	//one process
	//handling one connection at a time

//Concurrent:i existing, happening or done at the same time
	//one or more processes
	//handling multiple connections concurrently

//Preemptive: acquire or appropriate in advance; prevent something from happening
	//context: preemptive multitasking is task in which a computer OS uses some criteria to decide how long to allocate to any one task before giving another task to use the OS.
	//	 : The act of taking control of the OS from one task and giving it to another task is called: preempting
	//	 : a common criterion for preempting is simply elapsed time (this kind of system is sometimes called time sharing or time slicing)
	// 	 : in some OS systems, some applications can be given higher priority than other applications, giving the higher priority programs control as soon as they are initiated and perhaps longer slices of time

	//context: non preemptive: server response time dependent on length of previous client's request: time is round robin to each process: server does not time slice between processes (need non blocking i/o)
	//	 : preemptive: server response time is not dependent on length of previous client's request: time is round robin to each process: server time slices between processes, server does time slice between processes

//Iterative Servers:
	//Non-preemptive:
		//client must wait for all previous requests to complete
		//easy to design, implement and maintain
		//best when: request processing time is short
		//no i/o needed by server

//Concurrent Servers:
	//many cliecnts may connect at once
	//want to minimize: response time, complexity
	//want to maximize: throughput (connections serviced per second)
	//hardware utilizations (%cpu usage)
	//tradeoffs: response time vs. throughput vs hardware utilization

	//Concurrency can be provided in 2 ways:
		//Apparent concurrency:	1 thread, no preemption, using non blocking i/o
			//single thread of execution using the select() and non blocking i/o (blocking i/o would prevent jumping to different processes between processing
			//whenever an i/o request would block: switch to another connection
			//up to a certain number of connections: maximizes CPU utilization, increases throughput
			//less programmin/debuggin problems than true concurrency
			//code is more complicated
			//works well only if requests are short


		//Real concurrency: multi thread, preemptive, 
			//multiple threads of execution:
				//could be multiple processes, each with one thread
			//OS preempts thread/process after each quantum (aka time step)
			//Preemptive: response time not dependent on length of previous client's request
			//harder to design, implement and maintain
			//up to a certain number of connections: maximizes CPU utilization , maximizes response time, increases throughput
			//after too many concurrent connections: everything gets worse (server eventually hangs), need to put limits on concurrent connections

	// 4 different methods:
		//1) create process per client: 1 process per client connection
			//Fork solution #1:
				//fork a new process to handle every connection
				//advantages: simple: minimal shared state to manage sinces processes do not share their info back and forth
				//disadvantages: process creation (fork) is slow; context switching between processes (i.e., transitioning the line of execution between a thread or between proceses) is also slow (but only minor compared to fork)

		//2) a pool of available processes
			//Fork solution #2:
				//maintain a pool of iterative processes to handle connections
				//advantages: no longer have to fork; have rapid response as long as there is an idle process available; can set the pool size so that you don't overload the hardware
				//disadvantages: process context switching; what to do if you get more connections than pool size

		//Threads allow multiple concurrent execution contexts within a single process
			//shared address space, shared code, shared data, etc.
				//can implement a web server as a single process with multiple threads:
					//either one thread per connection or a pool of threads
			//advantages: can sometimes avoid context-switching (user-level threads); can share data easily
			//disadvantages: code must be thread safe (aka re-entrant); must always worry about inadvertent data-sharing 
				//re-entrant: code (function/subfunction) that can be interrupted in the middle of its execution and then safely called again (re-entered) before its previous invocations complete execution
		//3) 1 process, create 1 thread per connection
		//4) 1 process, a pool of available threads
//Procedure:
        //Create a network endpoint with socket()
        //Bind socket to a port with bind()
        //Start listening for connections with listen()
        //Loop and accept connections with accept()
        //Read and write data to a client with send(), recv(), read(), and write()
