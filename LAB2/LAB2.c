#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> 
#include <unistd.h>
#include <string.h>


#define	MAX_PENDING	1							// max pending client connections
#define	BUFFER_SIZE  	1024							// for incomming
#define ARG_ERROR_MESS	"CLIENT_MODE:\nclient <ip> <port> <filename or full filepath>\n\nSERVER_MODE:\nserver <ip> <port>"

int workSock, listenSock;							// socket descriptors
int ind = 0;									// indicator that any socket in open
long long	fileSize = 1;
long long 	filePointer = 0;


void hdl_SIGINT(int sig, siginfo_t *siginfo, void *context)			// handler for SIGINT (Ctrl+C) signal
{
    if (sig==SIGINT)
    {	 
	if(close(listenSock) < 0)			
		perror("sgn close listenSock");
	else
		ind-=2;
	if(ind)   
	{
		if(close(workSock) < 0)		
			perror("sgn close workSock");	
		else
			ind--;
	}
    }
}

void hdl_SIGTSTP(int sig, siginfo_t *siginfo, void *context)			// handler for SIGTSTP (Ctrl+Z) signal
{
  if(sig==SIGTSTP)
  {
    uint8_t buf = 1;
    send(workSock, &buf, sizeof(buf), MSG_OOB);
      perror("func send SIGTSTP");      
    puts("signal SIGTSTP. OOB data sended");
  }
}

void hdl_SIGURG(int sig, siginfo_t *siginfo, void *context)			// handler for OOB data received signal
{
  if(sig==SIGURG)
  {
    uint8_t buf = 1;
    recv(workSock, &buf, sizeof(buf), MSG_OOB);
      perror("func send SIGUSR1");      
    puts("signal SIGURG. OOB data received");
    printf("%lld bytes for download left\n", (fileSize - filePointer));
  }
}


int startServer(char *hostName, char *port)
{

	struct 		sockaddr_in hostAddr;    				// this machine 
	char 		buf[BUFFER_SIZE];					// buffer for incomming
	int 		readBytes;						// count of	
	int 		so_reuseaddr = 1;					// for setsockop SO_REUSEADDR set enable
	int 		clientFirstPacket;					// client first packet indicator
	FILE*		file;							
	char*		filePath = (char*) calloc (2, sizeof(char));

    	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));					// convert host byte order -> network byte order		
	if(!htons(inet_aton(hostName, hostAddr.sin_addr.s_addr)))		// new func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	{
		perror("func inet_aton");
        	return -1;		
	}
    	//hostAddr.sin_addr.s_addr = inet_addr(hostName);			// old func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	
	if(((listenSock) = socket(AF_INET, SOCK_STREAM, 0)) < 0)		
   	{
        	perror("func socket");
        	return -1;
    	}    
	ind+=2;
	setsockopt(listenSock, SOCK_STREAM, 
						SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
										// reuse ADDR when socket in TIME_WAIT condition
    	if(bind((listenSock), (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0)
										// socket is associated with IPv4 address
    	{
        	perror("func bind");
        	return -1;
    	}
    	if(listen((listenSock), MAX_PENDING) < 0)				// set socket to listen status
	{
		perror("func listen");
        	return -1;		
	}
	fd_set temp;
	FD_ZERO (&temp);
	FD_SET (workSock,&temp);	
	struct timeval time_out; time_out.tv_sec = 10; time_out.tv_usec = 0;
	while(1)								// if any key is pressed -> exit from while loop
    	{		
		clientFirstPacket = 1;
		if(((workSock) = accept((listenSock), 0, 0)) < 0)		// wait for client
		{
			perror("func accept");
            		return -1;    
        	}
		ind++;		
       		while(filePointer < fileSize)							
        	{			
			printf("+\n");			
			if(select(0,NULL,NULL,&temp,&time_out) < 0)
			{
				puts("server timeout 10s reached");
				break;
			}          
			if((readBytes = recv((workSock), buf, BUFFER_SIZE, MSG_WAITALL)) < 0)
										// receive data from client
			{
				perror("func recv");
				return -1;
			}
			if(!readBytes)
				break;
			if(clientFirstPacket)					// check if file exist (filename gets from client message) 
			{			  				
			  clientFirstPacket = 0;
			  filePath = (char*) realloc (buf, (readBytes + 1)*sizeof(char));	
			  filePath[readBytes] = '\0' ;
			  if(access(filePath, F_OK ) < 0)
			  {							// file not exist			    
			    file = fopen(filePath, "w+");			// create file
			    filePointer = 0;					
			  }
			  else			  			  				
			  {							// file exist
			    printf("Resume downloading %s file? Y/N\n", filePath);
			    if(getchar() != 'Y')
			      return -1;  
			  }
			  if((readBytes = recv((workSock), (char*)&fileSize, sizeof(long long), MSG_WAITALL)) < 0)
										// receive fileSize from client
			  {
				perror("func recv");
				return -1;
			  }
			  if(!readBytes)
				break;
										// send num bytes already received
			  if(send(workSock,(char*)&filePointer, sizeof(long long), 0) < 0)										
			  {
			    perror("func send");
			    return -1;
			  }   
			}	  
			else
			{
			  fwrite((char*)buf, readBytes, 1, file);
			  filePointer += readBytes;
			}
		}
		fclose(file);										
		if(close((workSock)) < 0)					// close connection
		{
			perror("func close workSock");
			return -1;
		}	
		ind--;
		printf("-\n");	  
	}
	return 0;	
}

int startClient(char *hostName, char *port, char *filePath)
{
  
	struct 		sockaddr_in hostAddr;    				// this machine 
	char 		buf[BUFFER_SIZE];					// buffer for outcomming
	int 		readBytes;						// count of	
	int 		so_reuseaddr = 1;					// for setsockop SO_REUSEADDR set enable
	FILE*		file;							
	if(((listenSock) = socket(AF_INET, SOCK_STREAM, 0)) < 0)		
   	{
        	perror("func socket");
        	return -1;
    	}    
	ind+=2;
    	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));					// convert host byte order -> network byte order		
	if(!htons(inet_aton(hostName, hostAddr.sin_addr.s_addr)))		// new func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	{
		perror("func inet_aton");
        	return -1;		
	}
    	//hostAddr.sin_addr.s_addr = inet_addr(hostName);			// old func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	setsockopt(listenSock, SOCK_STREAM, 
						SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
										// reuse ADDR when socket in TIME_WAIT condition
	
	
										
	
	file = fopen(filePath, "r");						// open file for read
	fseek(file, 0L, SEEK_END);						
	int fileSize = ftell(file);						// get file size
	fseek(file, 0L, SEEK_SET);						
	fd_set temp;
	FD_ZERO (&temp);
	FD_SET (listenSock,&temp);	
	struct timeval time_out; time_out.tv_sec = 10; time_out.tv_usec = 0;
	while(1)								// if any key is pressed -> exit from while loop
    	{		
		if(connect(listenSock, (struct sockaddr*) &hostAddr, sizeof(hostAddr)) < 0)
		{
		  perror("func connect");
		  return -1;		
		}
		if(send(listenSock, (char*)filePath, strlen(filePath), 0) < 0)// send filePath
		{
		  perror("func send filePath");
		  return -1;
		}   								// send fileSize
		if(send(listenSock, (char*)&fileSize, sizeof(long long), 0) < 0)
		{
		  perror("func send fileSize");
		  return -1;
		} 								// get filePointer from client
		if(recv(listenSock,(char*)&filePointer, sizeof(long long), MSG_WAITALL) < 0)
		{									      			  
		  perror("func recv filePointer");
		  return -1;
		}	
		fseek(file, filePointer, SEEK_CUR);				
		while((readBytes = fread((char*)buf, BUFFER_SIZE, 1, file )) > 0 && (fileSize-filePointer))
		{
		  if(select(0,NULL,&temp,NULL,&time_out) < 0)												//Проверяем, имеются ли внеполосные данные
		  {
		    puts("server timeout 10s reached");
		    break;
		  }   		  
		  if(send(listenSock, (char*)&buf, readBytes, 0) < 0)
		  {
		    perror("func send fileFragment");
		    return -1;
		  }  
		}
		if(close(listenSock) < 0)			
		  perror("sgn close listenSock");
		fclose(file);							
	}
  return 0;
}			

int main(int argc, char *argv[])
{
	if(argc < 4)
	{
		puts(ARG_ERROR_MESS);
		perror("invalid command-line arguments");
		return -1;
	}	
	struct sigaction closeTerm, sendOOB, recvOOB;
	closeTerm.sa_sigaction =&hdl_SIGINT;
	closeTerm.sa_flags = SA_SIGINFO;
	if(sigaction(SIGINT, &closeTerm, NULL) < 0)					// set handler for SIGINT signal (CTRL+C)
	{
		perror("main sigaction closeTerm");
		return -1;
	}	
	if(!strcmp(argv[1], "server"))	
	{
	  recvOOB.sa_sigaction =&hdl_SIGURG;
	  recvOOB.sa_flags = SA_SIGINFO;
	  if(sigaction(SIGURG, &recvOOB, NULL) < 0)					// set handler for SIGURG signal
	  {
		  perror("main sigaction recvOOB");
		  return -1;
	  }
	  startServer(argv[1], argv[2]);	
	}
	if(!strcmp(argv[1], "client"))
	{										
	  if(access(argv[4], F_OK ) < 0)						// check file exist
	  {
	    printf("file %s does not exist\n", argv[4]);
	    perror("invalid fileName");
	    return -1;
	  }
	  sendOOB.sa_sigaction =&hdl_SIGTSTP;
	  sendOOB.sa_flags = SA_SIGINFO;
	  if(sigaction(SIGTSTP, &sendOOB, NULL) < 0)					// set handler for SIGTSTP signal (CTRL+Z)
	  {
		  perror("main sigaction sendOOB");
		  return -1;
	  }
	  startClient(argv[1], argv[2], argv[3]);
	}
	if(ind > 1)
	{
		if(close(listenSock) < 0)		
		{
			perror("main close listenSock");	
			return -1;
		}
		ind-=2;
	}
	if(ind)
	{
		if(close(workSock) < 0)		
		{
			perror("main close workSock");	
			return -1;					
		}
	}
    	return 0;
}
