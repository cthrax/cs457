#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

// Holds data structs
#include "awget.h"

// Default filename, if no file name is found in URL
const char *DefaultFile = "index.html";

void *ss_func(void *file_desc); /*Thread Function*/

const int VERSION = 1;

// Ephemeral Port Range, low and hi values
const int LOWER_IP = 49152;
const int UPPER_IP = 65535;

/* Global Declarations for Thread */
int prt_no; // Port No. from command line
char myip[15] = ""; // char array to store SS ip
uint32_t myip_addr; // Integer of myip for comparison
socklen_t sin_size; // Used in inet_ntop()

int main(int argc, char *argv[]) {
    int c; // Input for getopt()

    // Checking Number of Input Arguments
    opterr = 0;

    if (argc == 1) {
        fprintf(stderr, "Must specify port.\n");
        return 1;
    } else if (argc != 3) {
        fprintf(stderr, "Format: -p <port number from 49152-65535>\n");
        return 1;
    }

    // Sanity Check and Command Line Argument Parsing
    while ((c = getopt(argc, argv, "p:")) != -1) {
        switch (c) {
        case 'p':
            prt_no = atoi(optarg);
            if (prt_no < LOWER_IP || prt_no > UPPER_IP) {
                fprintf(
                        stderr,
                        "Invalid port %s. Use ephemeral port within 49152-65535 to prevent stepping on registered ports.\n",
                        optarg);
                return 1;
            }
            break;
        case '?':
            if (optopt == 'p') {
                fprintf(
                        stderr,
                        "Option -%c requires an argument (-p <port number from 49152-65535>)\n",
                        optopt);
            }
            return 1;
        default:
            abort();
        }
    }

    // Defining Structures used
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in their_addr; // connector's address information

    // Socket Creation
    struct sockaddr_in sock;
    struct hostent *h;

    int sockfd; // Socket for listening
    int sd; // Socket file descriptor for accepting

    char ip[50] = "";
    //    char buf[10]="";
    int num = 0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    // Getting Host Name	( local SS IP Address by name and by IP)
    int gh = gethostname(ip, sizeof(ip));
    if (gh == -1) {
        perror("gethostname");
        exit(1);
    }
    h = gethostbyname(ip);
    if (h == NULL) {
        perror("gethostbyname");
        exit(1);
    }

    // SS IP in dotted quad notation
    strcpy(myip, inet_ntoa(**((struct in_addr **)h->h_addr_list)));

    // Getting integer value of myip
    struct sockaddr_in temp;
    inet_pton(AF_INET, myip, &(temp.sin_addr));
    myip_addr = temp.sin_addr.s_addr;


    memset(&sock, 0, sizeof sock);
    sock.sin_family = AF_INET;
    sock.sin_addr.s_addr = INADDR_ANY;
    sock.sin_port = htons(prt_no);

    // Socket creation
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // Binding the Socket
    if (bind(sockfd, (struct sockaddr *) &sock, sizeof(sock)) == -1) {
        close(sockfd);
        perror("bind:");
        exit(1);
    }

    // Initialise a thread with id SS //why?
    pthread_t ss;

    if (listen(sockfd, 10) == -1) {
        close(sockfd);
        perror("listen");
        exit(1);
    }
    printf("Server listening on TCP %s, Port %d\n", myip, prt_no);

    while (1) {
        //Accept the Incoming request from the Client
        sd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
        if (sd < 0) {
            close(sd);
            perror("Accept");
            exit(1);
        }

        if ((pthread_create(&ss, NULL, ss_func, (void *) &sd)) != 0) {
            close(sd);
            perror("pthread_create");
            exit(1);
        }

        printf("\nAwaiting More Connections on TCP %s, Port %d\n", myip, prt_no);
    }
}//Main Function Close


//================================================================================================================//
/* Thread Function, Handles Incoming Requests from SS and AWGET, wget's the file and sends it back to AWGET */
//================================================================================================================//
/* Thread to Handle SS Jobs*/
void *ss_func(void *file_desc) {
    int *filedesc = (int *) file_desc;
    int fd = *filedesc;

    char ssbuff[15000];//The sending *could* be this large w/ a long chainfile, But it probably won't be.
    // Read the Input from the Client
    int packet_size = sizeof(struct ss_packet);
    int siz = 0;
    siz += recv(fd, ssbuff, packet_size, MSG_WAITALL);
    if ( siz < 0) {
        close(fd);
        perror("Receive");
        pthread_exit(NULL);
    }

    // Code to dememcpy the attributes
    struct ss_packet *ssp = (struct ss_packet*) ssbuff;
////
    printf("size got = %d\n", siz);
    printf("version: %d\n", ssp->version);
////

    /* Checking for version */
    if (ssp->version != VERSION) {
        printf("Version Mismatch, Dropping Packet and Exiting.\n");
        close(fd);
        pthread_exit(NULL);
    }
    int len = strlen(ssp->url);
    printf("len = %d\n", len);
    char *ss_url = ssp->url;
    printf("decrement stepcount\n");//TODO remove
    ssp->step_count--;

    if (ssp->step_count == 0) // Special case -> wget the file from url!
    {
        printf("I'm the getter!\n");
        struct stat st;
        char file_buff[4000];

        long int fsize = 0;
        // Download the File
        fprintf(stderr, "%s\n",ss_url);
        sprintf(file_buff,"wget -q %s",ss_url);
        printf("Request: %s\n", ss_url);
        printf("\nIssuing wget to download the File from URL...\n\n");
        system(file_buff);

        const char *filename;
        char *f_ptr;
        f_ptr = strrchr(ss_url, '/');

        if (f_ptr == NULL) {
            filename = DefaultFile;
        } else {
            f_ptr++;
            filename = (char *) f_ptr;
        }
        printf("filename: %s \n", filename);

        // Finding size of downloaded file
        if (stat(filename, &st) == 0) {
            fsize = st.st_size;
            //   	    printf("File Size : %ld\n", fsize);
        } else {
            printf("File Size Stat\n");
            close(fd);
            pthread_exit(NULL);
        }

        char *filebuff;
        filebuff = (char *) malloc(fsize);

        int open_fd = open(filename, O_RDONLY, 0666);
        if ((read(open_fd, filebuff, fsize) <= 0)) {
            close(fd);
            perror("File Read:");
            pthread_exit(NULL);
        }

        close(open_fd);

        //remove the downloaded file
        char rem_file[128] = "";
        sprintf(rem_file,"rm %s", filename);
        system(rem_file);

        //		filebuff[fsize]='\0';

        char size[10] = "";
        long int siz = htonl(fsize);
        memcpy(size, &siz, sizeof(siz));

        if ((send(fd, size, 10, 0)) == -1) {
            close(fd);
            perror("File Size Send");
            pthread_exit(NULL);
        }

        // Sending File Data
        long int bytes = 0;
        if ((bytes = send(fd, filebuff, fsize, 0)) == -1) {
            close(fd);
            perror("Data Send");
            pthread_exit(NULL);
        }
        printf("\nRelaying File\n");
        close(fd);
        printf("Finished sending file. Awaiting more connections.\n");
        pthread_exit(NULL);
    }

    // Scenario where there is more than 1 hop left
    else {

        // Check for local IP and Chainlist
	printf( "url is %s\n", ssp->url);
        int count = 0, iFor = 0;
	printf(" ss - starting list %d\nMy info: %s, %d\n", ssp->step_count, myip, prt_no);
        for (iFor = 0; iFor < ssp->step_count+1; iFor++) { 
            printf(" inside for loop: %d with cur = %d and %d \n", iFor, ntohl(ssp->steps[iFor].ip_addr), ntohs(ssp->steps[iFor].port_no));
            uint32_t tip = ntohl(ssp->steps[iFor].ip_addr);
            uint16_t tp = ntohs(ssp->steps[iFor].port_no);
            if ((tip == myip_addr && tp != prt_no) || (tip != myip_addr)) {
                printf(" Not equal! ip: %d, port: %d\n", ntohl(ssp->steps[iFor].ip_addr), ntohs(ssp->steps[iFor].port_no));
                ssp->steps[count].ip_addr = ssp->steps[iFor].ip_addr;//count?
                ssp->steps[count++].port_no = ssp->steps[iFor].port_no;
            }
        }

        //Clear out last element
        ssp->steps[iFor].ip_addr = 0;
        ssp->steps[iFor].port_no = 0;

        printf(" ss - copied ssp->steps\n");
        printf("chainlist is \n");

        int iLoop = 0;

        // picking a random ss from the list
        int rand_ss;
        srand(time(NULL));
        rand_ss = rand() % ssp->step_count;

        char nextIp[INET_ADDRSTRLEN];
        uint16_t nextPort = 0;
        int nextIdx = 0;

        for (iLoop = 0; iLoop < ssp->step_count; iLoop++) {
            struct int_tuple* cur = (ssp->steps + iLoop);
            char ip[INET_ADDRSTRLEN];
            struct in_addr temp;
            temp.s_addr = ntohl(cur->ip_addr);
            uint16_t port = ntohs(cur->port_no);
            inet_ntop(AF_INET, &(temp), ip, INET_ADDRSTRLEN);
            printf("<%s, %d>\n", ip, port);

            if (iLoop == rand_ss) {
                strcpy(nextIp, ip);
                nextPort = port;
                nextIdx = iLoop;
            }
        }

        printf("\nNext SS is <%s, %d>\n", nextIp, nextPort);

        // Server Address
        int cli_ss = 0;
        struct addrinfo hints, *server_sck_addr, *p;
        int rv = 0;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char sPort[6];
        sprintf(sPort, "%d", nextPort);
        if ((rv = getaddrinfo(nextIp, sPort, &hints, &server_sck_addr)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            pthread_exit(NULL);
        }

        // loop through all the results and connect to the first we can
        for(p = server_sck_addr; p != NULL; p = p->ai_next) {
            if ((cli_ss = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }

            if (connect(cli_ss, p->ai_addr, p->ai_addrlen) == -1) {
                close(cli_ss);
                perror("client: connect");
                continue;
            }

            break;
        }

        printf("Sending packet...\n");
        // Send Packet to data SS
        ssize_t bytesSent = 0;
        size_t packetSize = sizeof(struct ss_packet);
        bytesSent += send(cli_ss, &(*ssp), sizeof(struct ss_packet), MSG_WAITALL);

        if (bytesSent == -1) {
            close(cli_ss);
            close(fd);
            perror("Next SS Send");
            pthread_exit(NULL);
        }

        printf("Sent %lu bytes, expect to send %lu\n", (unsigned long int)bytesSent, (unsigned long int)packetSize);

        //recv file size
        char sizebuff[10] = "";
        int frecv_bytes = 0;

        printf("Receiving filesize.\n");
        // Read the file size from next SS
        if ((frecv_bytes = recv(cli_ss, sizebuff, 10, 0)) == -1) {
            close(cli_ss);
            close(fd);
            perror("File Size Receive");
            pthread_exit(NULL);
        }

        printf("Sending filesize.\n");
        // Send File Size to previous SS/Awget
        if ((send(fd, sizebuff, frecv_bytes, 0)) == -1) {
            close(cli_ss);
            close(fd);
            perror("File Size Send:");
            pthread_exit(NULL);
        }

        // Hanlde File Size
        long int filesiz = ntohl(filesiz);

        // malloc() filesize for receiving file data
        char *databuff = (char*) malloc(filesiz);

        // Read the file data from the SS
        if (recv(cli_ss, databuff, filesiz, MSG_WAITALL) == -1) {
            close(cli_ss);
            close(fd);
            perror("Data Receive");
            pthread_exit(NULL);
        }

        close(cli_ss);
        printf("\nRelaying File\n");

        // Send the file data to previous SS/AWGET
        if ((send(fd, databuff, filesiz, 0)) == -1) {
            close(fd);
            perror("Data Send:");
            pthread_exit(NULL);
        }

        close(fd);
    }
    pthread_exit(NULL);
}
