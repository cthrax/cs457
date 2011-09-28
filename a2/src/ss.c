/*
Sudharshan Varadarajan
Date: 09/25/2011
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

/* 
1. ss acts as server and gets the url and packet from awget client
2. Decrypts the packet gets the necessary info. and forward to the next ss 
3. Once the hop is zero, get the file from url specified and send it back to the awget client in the same route.
*/

/*
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
*/

// Structure declarations to receive from awget // 
struct ss_packet
{
		uint8_t version;
		uint16_t ss_no;
		uint16_t url_len;
}__attribute__((__packed__));

// <<IP,Port>> Pair Sturcture
struct int_tuple
{
	uint32_t ip_addr;
	uint32_t port_no;
}__attribute__((__packed__));

//Getting char IP - (used in bnding process)
struct char_ip
{
		char ch_ip[16];
}__attribute__((__packed__));
	
int main(int argc, char *argv[])
{
	// Defining Structures used
	struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in their_addr; 		// connector's address information
	// Socket Creation
	struct sockaddr_in ss_sck_addr;
	struct sockaddr_in sock;
	struct hostent *h;
	
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    int i = 0;
    char c; 
    socklen_t sin_size;
    char str[INET_ADDRSTRLEN];   
    char myip[15], ip[50]=""; 
    char buf[10]="";
    int num = 0;
    int rv;
    int prt_no;
    int sd;
//    string str_prot ;
//	string strT = "tcp";
		
    memset(&hints, 0, sizeof hints);   
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

	
    // Checking Number of Input Arguments
	if (argc <1)
	{
		printf("Enter Valid Arguments\n");
		exit(1);
	}
	
	// Sanity Check and Command Line Argument Parsing
	while ((c = getopt (argc, argv, "p:")) != -1)
	{
		switch (c)
		{     
        case 'p':
        prt_no=atoi(optarg);
        printf("Port Number: %d\n",prt_no);
        break;


        default: 	
        abort ();
      }
    }
	
	// Getting Host Name	( local SS IP Address by name and by IP)
	int gh = gethostname(ip, sizeof(ip));
	if(gh == -1)
	{
		perror("gethostname");
		exit(1);
	}
	h = gethostbyname(ip);
	if(h == NULL)
	{
		perror("gethostbyname");
		exit(1);
	}

	//myip is local SS IPv4 address
//	myip = (char *) malloc(sizeof (char));
	strcpy(myip, inet_ntoa(**((struct in_addr **)h->h_addr_list)));

	memset(&sock, 0, sizeof sock);
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = INADDR_ANY;
	sock.sin_port = htons(prt_no);
	
	// Socket creation
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    	   	{
				perror("socket");
    	   	 	exit(1);
			}
	// Binding the Socket
	if (bind(sockfd, (struct sockaddr *)&sock, sizeof(sock)) == -1)
	{
		close(sockfd);
		perror("bind:");
	}

	while(1)
	{
		printf("Server listening on %s\n", myip);
		printf("SS listening on %d", prt_no); 
   		if (listen(sockfd, 10) == -1)
    	{
        	perror("listen");
			exit(1);
		}   
		
		//Accept the Incoming request from the Client
		sd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if(sd < 0)
		{
			perror("Accept");
			exit(1);
		}
		//	inet_ntop( AF_INET, &their_addr, str, INET_ADDRSTRLEN );
		strcpy(str, inet_ntoa(their_addr.sin_addr));
//		cout<<"Server : Incoming Connection from " << str << endl;

		char ssbuff[1024]="";
		// Read the Input from the Client
		if (recv(sd, ssbuff, sizeof(ssbuff), 0) < 0)
		{
			perror("Receive");
			exit(1);
		}
	
		// Code to dememcpy the attributes
		int demem_buff ;
		int mem_off=0;
		int hop;
		struct ss_packet ssp;
	
		memcpy(&ssp.version, ssbuff+mem_off, sizeof(ssp.version));
		mem_off+=sizeof(ssp.version);
		
		/* Check for version - implement */
	
		memcpy(&ssp.ss_no, ssbuff+mem_off, sizeof(ssp.ss_no));
		mem_off+=sizeof(ssp.ss_no);
		hop = ntohs(ssp.ss_no);
		printf("no.of ss = %d\n", hop);
		hop--;

		// Structure Instanciation
		struct int_tuple poip[ntohs(ssp.ss_no)];
		struct char_ip chip[ntohs(ssp.ss_no)];
		
		struct int_tuple ssip[hop];
		struct char_ip nxt_ip[hop];
			
		memcpy(&ssp.url_len, ssbuff+mem_off, sizeof(ssp.url_len));
		mem_off+=sizeof(ssp.url_len);
		int len = ntohs(ssp.url_len);
		printf("url_len = %d\n",len);
	
		char ss_url[len];
		memcpy(&ss_url, ssbuff+mem_off, len);
		mem_off+=len;
		ss_url[len] = '\0';
		printf("URL = %s\n", ss_url);
		
		for(i=0; i<hop+1; i++)
		{
			memcpy(&poip[i].ip_addr, ssbuff+mem_off, sizeof(poip[i].ip_addr));
			mem_off+=sizeof(poip[i].ip_addr);
			inet_ntop(AF_INET, &poip[i].ip_addr, chip[i].ch_ip, sin_size);
			printf("ip[%d] = %s\t", i, chip[i].ch_ip);
			
	
			memcpy(&poip[i].port_no, ssbuff+mem_off, sizeof(poip[i].port_no));
			mem_off+=sizeof(poip[i].port_no);
			printf("port[%d] = %d\n", i, poip[i].port_no);
		}

		// SS as client
		// Common attributes for any number of SS
		
		char sendbuff[1024] ="";
		mem_off = 0;
		memcpy(sendbuff, &ssp.version, sizeof(ssp.version));
		mem_off+=sizeof(ssp.version);
		
		hop = htons(hop);
		memcpy(sendbuff+mem_off, &hop, sizeof(ssp.ss_no));
		mem_off+=sizeof(ssp.ss_no);
		hop = ntohs(hop);
		
		memcpy(sendbuff+mem_off, &ssp.url_len, sizeof(ssp.url_len));
		mem_off+=sizeof(ssp.url_len);
		memcpy(sendbuff+mem_off, &ss_url, ntohs(ssp.url_len));
		mem_off+=ntohs(ssp.url_len);
	
		if(hop == 0) // Special case -> get the data from url!
		{
		
			char file_buff[1024]="";
			
			// Get the file from url 
			sprintf(file_buff,"wget -b -o log %s",ss_url); 			
			printf("file content : %s\n", file_buff);	
			system(file_buff);

			
			/*printf("Next SS is <%s, %d>\n", chip[0].ch_ip, poip[0].port_no);

			memcpy(sendbuff+mem_off, &poip[0].ip_addr, sizeof(poip[0].ip_addr));
			mem_off+=sizeof(poip[0].ip_addr);
		
			memcpy(sendbuff+mem_off, &poip[0].port_no, sizeof(poip[0].port_no));
			mem_off+=sizeof(poip[0].port_no);
		
			// Server Address
			ss_sck_addr.sin_family = AF_INET;						// to use both IPv4 or IPv6
			ss_sck_addr.sin_addr.s_addr = inet_addr(chip[0].ch_ip);	// use TCP
			ss_sck_addr.sin_port = htons(poip[0].port_no);			// Use my IPaddr.s_addr*/
		}
		else
		{
			// Check for local IP and Chainlist
				int count = 0;
				uint32_t myip_addr;
				inet_pton(AF_INET, myip, &myip_addr);
				printf("myIP_ss = %d\n", myip_addr);
				
				for(i=0; i<hop+1; i++)
				{
					printf("MYIP %d\t poip %d\n", myip_addr, poip[i].ip_addr);
					if(myip_addr != poip[i].ip_addr)
					{
						memcpy(sendbuff+mem_off, &poip[i].ip_addr, sizeof(poip[i].ip_addr));
						mem_off+=sizeof(poip[count].ip_addr);
						memcpy(&ssip[count].ip_addr, &poip[i].ip_addr, sizeof(poip[i].ip_addr));
						memcpy(nxt_ip[count].ch_ip, &chip[i].ch_ip, sizeof(chip[i].ch_ip));
			
						memcpy(sendbuff+mem_off, &poip[i].port_no, sizeof(poip[i].port_no));
						mem_off+=sizeof(poip[i].port_no);
						memcpy(&ssip[count].port_no, &poip[i].port_no, sizeof(poip[i].port_no));
						
						count++;
					}
					else if(myip_addr == poip[i].ip_addr)
					{
						if(prt_no != poip[i].port_no)
						{
							memcpy(sendbuff+mem_off, &poip[i].ip_addr, sizeof(poip[i].ip_addr));
							mem_off+=sizeof(poip[i].ip_addr);
							memcpy(&ssip[count].ip_addr, &poip[i].ip_addr, sizeof(poip[i].ip_addr));
							memcpy(nxt_ip[count].ch_ip, &chip[i].ch_ip, sizeof(chip[i].ch_ip));
				
							memcpy(sendbuff+mem_off, &poip[i].port_no, sizeof(poip[i].port_no));
							mem_off+=sizeof(poip[i].port_no);
							memcpy(&ssip[count].port_no, &poip[i].port_no, sizeof(poip[i].port_no));
							
							count++;
						}
					}
				}
			printf("chainlist is \n");
			for(i=0; i<hop; i++)
			{
				printf("<%s(%d), %d>\n", nxt_ip[i].ch_ip, ssip[i].ip_addr, ssip[i].port_no);
			}
			
			// picking a random ss from the list
			int rand_ss;
			srand(time(NULL));
			rand_ss = rand() % hop;
			printf("Random SS = %d\n", rand_ss);		
			printf("Next SS is <%s, %d>\n", nxt_ip[rand_ss].ch_ip, ssip[rand_ss].port_no);
			
			// Server Address
			ss_sck_addr.sin_family = AF_INET;								// to use IPv4
			ss_sck_addr.sin_addr.s_addr =inet_addr(nxt_ip[rand_ss].ch_ip);	// use next ss ip
			ss_sck_addr.sin_port = htons(ssip[rand_ss].port_no);			// Use next ss port

			int cli_ss = 0;
			// Create a Socket for the Client(awget)	
			if((cli_ss = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			{
				perror("Socket");
				exit(1);
			}	
			
			// Conenct to SS
			if(connect(cli_ss, (struct sockaddr *)&ss_sck_addr, mem_off) != 0 )
			{
				perror("Connect");
				exit(1);
			}
	
			int numbytes;
			// Send Packet to SS
			if ((numbytes = send(cli_ss, sendbuff, mem_off, 0)) == -1)
			{
	    	    perror("Send:");
	    	    exit(1);
	    	}
	
			close(cli_ss);
    
			return 0;
		}
		
		// Copy the contents of the Packet  into the Buffer
		/*memcpy(&num, buf, sizeof(rep.version));
		cout << "Version Number = "<< num << endl;
	
		// Check for Correct Version
		if(num != rep.version)
//	if(ntohs(num) != rep.version)
	{
			cout<<"Version Error" << endl;
			exit(1);
	}
	else		// If the Version is Correct, dispaly the version on the screen
	{
		memcpy(&num, buf+sizeof(rep.version), (sizeof(rep.version)+sizeof(rep.version)));
		cout<<" The number is :" << ntohs(num) <<endl;
	}
	
	// Send the Version back to Client through the Socet Descriptor
	rep.version = rep.version;
//	rep.version = htons(rep.version);
//	sleep(5);
	if(send(sd, &rep.version, sizeof(rep.version),0) <0)
	{
		perror("Send");
		exit(1);
	}
	
	rep.version= rep.version;
//	rep.version=ntohs(rep.version);*/
	
	// Closing the Server Connection
	close(sd);
	//}
	}// While loop closed
	//}
    return 0;
    
    // else for UDP
    
    /*	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
       	{
			perror("socket");
       	 	exit(1);
		}*/
}
