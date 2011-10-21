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
    inet_pton(AF_INET, myip, &myip_addr);

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

    char ssbuff[15000] = "";//The sending *could* be this large w/ a long chainfile, But it probably won't be.
    // Read the Input from the Client
    if (recv(fd, ssbuff, 15000, 0) < 0) {
        close(fd);
        perror("Receive");
        exit(1);
    }

    // Code to dememcpy the attributes
    int mem_off = 0;
    int hop;
    struct ss_packet ssp;

    /* Checking for version */
    if (ssp.version != VERSION) {
        printf("Version Mismatch, Dropping Packet and Exiting.\n");
        close(fd);
        exit(1);
    }
    printf("first emmecpy!\n");

    int len = strlen(ssbuff + mem_off);
    printf("my len is \ %d \n", len);
    char* ss_url = (char*) malloc(len * sizeof(char) + 1);
    ssp.step_count--;

    if (ssp.step_count == 0) // Special case -> wget the file from url!
    {
        printf("I'm the getter!\n");
        struct stat st;
        char file_buff[384] = "";

        long int fsize = 0;
        // Download the File
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
            exit(1);
        }

        char *filebuff;
        filebuff = (char *) malloc(fsize);

        int open_fd = open(filename, O_RDONLY, 0666);
        if ((read(open_fd, filebuff, fsize) <= 0)) {
            close(fd);
            perror("File Read:");
            exit(1);
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
            exit(1);
        }

        // Sending File Data
        long int bytes = 0;
        if ((bytes = send(fd, filebuff, fsize, 0)) == -1) {
            close(fd);
            perror("Data Send");
            exit(1);
        }
        printf("\nRelaying File\n");
        close(fd);
    }

    // Scenario where there is more than 1 hop left
    else {
        // Structure Instanciation
        struct int_tuple *poip = (int_tuple*) malloc(
                sizeof(int_tuple *[hop + 1]));
        struct char_ip* chip = (char_ip*) malloc(sizeof(char_ip *[hop + 1]));

        struct int_tuple *ssip = (struct int_tuple*) malloc(sizeof(struct int_tuple) * ssp.step_count);
        struct char_ip nxt_ip = (char_ip*) malloc(sizeof(char_ip*[hop]));


        // Check for local IP and Chainlist
        int count = 0, iFor = 0;
        struct int_tuple *newSsList = (struct int_tuple*) malloc(sizeof(struct int_tuple) * ssp.step_count);

        for (iFor = 0; iFor < ssp.step_count + 1; iFor++) {
            struct int_tuple *cur = (ssp.steps + iFor);
            if (myip_addr != cur->ip_addr && prt_no != cur->portno) {
                (newSsList + count++) = cur;
            }
        }

        ssp.steps = newSsList;

        printf("chainlist is \n");

        int iLoop = 0;

        // picking a random ss from the list
        int rand_ss;
        srand(time(NULL));
        rand_ss = rand() % hop;

        struct int_tuple* nextSS;
        char nextIp[INET_ADDRSTRLEN];
        uint16_t nextPort = 0;

        for (iLoop = 0; iLoop < hop + 1; iLoop++) {
            struct int_tuple* cur = (verss.steps + iLoop);
            char ip[INET_ADDRSTRLEN];
            struct in_addr temp;
            temp.s_addr = ntohl(cur->ip_addr);
            uint16_t port = ntohs(cur->port_no);
            inet_ntop(AF_INET, &(temp), ip, INET_ADDRSTRLEN);
            printf("<%s, %d>\n", ip, port);

            if (i == rand_ss) {
                strcpy(nextIp, ip);
                nextPort = port;
                nextSS = cur;
            }
        }

        printf("\nNext SS is <%s, %d>\n", nextIp, nextPort);

        // Server Address
        struct sockaddr_in ss_sck_addr;
        ss_sck_addr.sin_family = AF_INET; // to use IPv4
        ss_sck_addr.sin_addr.s_addr = ntohl(nextSS.ip_addr); // use next ss ip
        ss_sck_addr.sin_port = ntohs(nextSS.port_no); // Use next ss port

        int cli_ss = 0;

        // Create a Socket for the next Stepping Stone
        if ((cli_ss = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            close(fd);
            perror("Next SS Socket");
            exit(1);
        }

        // Conenct to next Stepping Stone
        if (connect(cli_ss, (struct sockaddr *) &ss_sck_addr,
                sizeof(ss_sck_addr)) != 0) {
            close(cli_ss);
            close(fd);
            perror("Next SS Connect");
            exit(1);
        }

        // Send Packet to data SS
        if ((send(cli_ss, sendbuff, mem_2off, 0)) == -1) {
            close(cli_ss);
            close(fd);
            perror("Next SS Send");
            exit(1);
        }

        //recv file size
        char sizebuff[10] = "";
        int frecv_bytes = 0;

        // Read the file size from next SS
        if ((frecv_bytes = recv(cli_ss, sizebuff, 10, 0)) == -1) {
            close(cli_ss);
            close(fd);
            perror("File Size Receive");
            exit(1);
        }

        // Send File Size to previous SS/Awget
        if ((send(fd, sizebuff, frecv_bytes, 0)) == -1) {
            close(cli_ss);
            close(fd);
            perror("File Size Send:");
            exit(1);
        }

        // Hanlde File Size
        long int filesiz = 0;
        memcpy(&filesiz, sizebuff, frecv_bytes);
        filesiz = ntohl(filesiz);

        // malloc() filesize for receiving file data
        char *databuff = (char*) malloc(filesiz);

        // Read the file data from the SS
        if (recv(cli_ss, databuff, filesiz, MSG_WAITALL) == -1) {
            close(cli_ss);
            close(fd);
            perror("Data Receive");
            exit(1);
        }

        close(cli_ss);
        printf("\nRelaying File\n");

        // Send the file data to previous SS/AWGET
        if ((send(fd, databuff, filesiz, 0)) == -1) {
            close(fd);
            perror("Data Send:");
            exit(1);
        }

        close(fd);
    }
    pthread_exit(NULL);
}
