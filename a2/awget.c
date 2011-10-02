#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>
#include <time.h>

#if defined(__APPLE__)
#include <stdio.h>   /* flockfile, getc_unlocked, funlockfile */
#include <stdlib.h>  /* malloc, realloc */

#include <errno.h>   /* errno */
#include <unistd.h>  /* ssize_t */

ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif

struct ss_packet {
    uint8_t version;
    uint16_t ss_no;
    uint16_t url_len;
}__attribute__((__packed__));

// <<IP,Port>> Pair Sturcture
struct int_tuple {
    uint32_t ip_addr;
    uint32_t port_no;
}__attribute__((__packed__));

//Getting char IP
struct char_ip {
    char ch_ip[16];
}__attribute__((__packed__));

int main(int argc, char *argv[]) {
    /*struct hostent *h;
     struct sockaddr_in server_sck_addr;
     struct reply_packet rep_p;

     socklen_t server_addr_len = sizeof(server_sck_addr);

     // Variable Declarations
     int c_sd = 0, gai =0;									// sd -> Socket Descriptor
     int num, p_no =0, count = 0, numbytes =0;
     int val=0;
     string prot;
     char name[50]="";
     char *buf;
     char ssbuff[10]="";
     char rcbuff[4]="";
     rep_p.version = 1;
     //	rep_p.version = htons(1);
     char *serverip;*/

    int in_opt = 0;
    char url[128] = "", *chfile;

    //Structure Intialization
    struct ss_packet verss;

    if (argc != 5) {
        printf(
                "Enter Correct number of arguments in any order \n  %s -u <URL> -c <chain file> \n",
                argv[0]);
        exit(1);
    }

    //getopt() function to do SANIY CHECK FOR INPUTS
    while ((in_opt = getopt(argc, argv, "c:u:")) != -1) {
        switch (in_opt) {
        case 'c':
            //Getting Chain file Name
            chfile = optarg;
            break;

        case 'u':
            // Getting URL
            strcpy(url, optarg);
            break;

        default:
            abort();
        }
    }

    // Reading from Chain file
    FILE *fp;
    char *ip_addr;
    char *line;
    size_t line_len = 25;
    char *tok;

    int count = 0;
    int port_no = 0, num_ss = 0, num_bytes = 0;

    verss.version = 1;
    verss.url_len = strlen(url);
    printf("URL Length :%d\n", verss.url_len);

    // Opening a Chain File pointed by a File Pointer
    fp = fopen(chfile, "r");
    line = malloc(line_len);

    if (getline(&line, &line_len, fp) > 0) {
        tok = strtok(line, "\n");
        //		printf("Number of SS : %s\n", tok);
        verss.ss_no = atoi(tok);
    }

    if (verss.ss_no == 0) {
        printf("No.of SS in Chain file is Zero\n");
        exit(1);
    }

    struct char_ip cip[verss.ss_no];
    struct int_tuple itp[verss.ss_no];

    //  Getting IP,Port Tuple for each SS
    while ((getline(&line, &line_len, fp) > 0)) {
        tok = strtok(line, " ");
        //		printf("IP : %s\t", tok);
        strcpy(cip[count].ch_ip, tok);
        inet_pton(AF_INET, tok, &itp[count].ip_addr);

        tok = strtok(NULL, "\n");
        //		printf("Port : %s\n", tok);
        itp[count].port_no = atoi(tok);
        count++;
    }

    fclose(fp);

    int i;
    printf("No.of SS : %d\n", verss.ss_no);
    printf("Request:%s \n", url);

    // Memcpy to Buf before Sending
    char ssbuff[512] = "";
    int mem_offset = 0;
    int hop = verss.ss_no;
    verss.ss_no = htons(verss.ss_no);
    verss.url_len = htons(verss.url_len);

    // Common attributes for any number of SS
    memcpy(ssbuff, &verss.version, sizeof(verss.version));
    mem_offset += sizeof(verss.version);
    memcpy(ssbuff+mem_offset, &verss.ss_no, sizeof(verss.ss_no));
    mem_offset += sizeof(verss.ss_no);
    memcpy(ssbuff+mem_offset, &verss.url_len, sizeof(verss.url_len));
    mem_offset += sizeof(verss.url_len);
    memcpy(ssbuff+mem_offset, &url, ntohs(verss.url_len));
    mem_offset += ntohs(verss.url_len);

    // Socket Creation
    int cli_sd = 0;
    struct sockaddr_in server_sck_addr;

    // Create a Socket for the Client(awget)
    if ((cli_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket");
        exit(1);
    }

    if (hop == 1) {
        printf("Next SS is <%s, %d>\n", cip[0].ch_ip, itp[0].port_no);

        memcpy(ssbuff+mem_offset, &itp[0].ip_addr, sizeof(itp[0].ip_addr));
        mem_offset += sizeof(itp[0].ip_addr);

        memcpy(ssbuff+mem_offset, &itp[0].port_no, sizeof(itp[0].port_no));
        mem_offset += sizeof(itp[0].port_no);

        // Server Address
        server_sck_addr.sin_family = AF_INET; // to use both IPv4 or IPv6
        server_sck_addr.sin_addr.s_addr = inet_addr(cip[0].ch_ip); // use TCP
        server_sck_addr.sin_port = htons(itp[0].port_no); // Use my IPaddr.s_addr
    } else {
        printf("chainlist is \n");

        for (i = 0; i < hop; i++) {
            printf("<%s(%d), %d>\n", cip[i].ch_ip, itp[i].ip_addr,
                    itp[i].port_no);

            memcpy(ssbuff+mem_offset, &itp[i].ip_addr, sizeof(itp[i].ip_addr));
            mem_offset += sizeof(itp[i].ip_addr);

            memcpy(ssbuff+mem_offset, &itp[i].port_no, sizeof(itp[i].port_no));
            mem_offset += sizeof(itp[i].port_no);

        }
        // picking a random ss from the list
        int rand_ss;
        srand(time(NULL));
        rand_ss = rand() % hop;
        printf("Random SS = %d\n", rand_ss);
        printf("Next SS is <%s, %d>\n", cip[rand_ss].ch_ip,
                itp[rand_ss].port_no);

        // Server Address
        server_sck_addr.sin_family = AF_INET; // to use both IPv4 or IPv6
        server_sck_addr.sin_addr.s_addr = inet_addr(cip[rand_ss].ch_ip); // use TCP
        server_sck_addr.sin_port = htons(itp[rand_ss].port_no); // Use my IPaddr.s_addr
    }

    // Conenct to SS
    if ((connect(cli_sd, (struct sockaddr *) &server_sck_addr, mem_offset))
            != 0) {
        perror("Connect");
        exit(1);
    }

    int numbytes;
    // Send Packet to SS
    if ((numbytes = send(cli_sd, ssbuff, mem_offset, 0)) == -1) {
        perror("Send:");
        exit(1);
    }

    close(cli_sd);

    return 0;

    // Create a Socket for the Client
    /*if((c_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
     {
     perror("Socket");
     exit(1);
     }
     h = gethostbyname(name);
     if(h == NULL)
     {
     perror("gethostbyname");
     exit(1);
     }

     serverip = (char *) malloc(sizeof (char));
     serverip = inet_ntoa(**((struct in_addr **)h->h_addr_list));

     server_sck_addr.sin_family = AF_INET;						// to use both IPv4 or IPv6
     server_sck_addr.sin_addr.s_addr =inet_addr(serverip);		// use TCP
     server_sck_addr.sin_port = htons(p_no);						// Use my IPaddr.s_addr

     // Conenct to Server
     if((connect(c_sd, (struct sockaddr *)&server_sck_addr, server_addr_len)) != 0 )
     {
     perror("Connect");
     exit(1);
     }

     // Memcpy to Buf before Sending
     memcpy(ssbuff, &rep_p.version, sizeof(rep_p.version));
     memcpy(ssbuff+sizeof(rep_p.version), &rep_p.numb, sizeof(rep_p.numb));

     // Send Data
     if ((numbytes = send(c_sd, ssbuff, (sizeof(rep_p.version)+sizeof(rep_p.numb)), 0)) == -1)
     {
     perror("Send:");
     exit(1);
     }
     cout << "Sent number to server IP:"<<serverip<< " port:"<<p_no<<" via " <<prot<< endl;

     // Time out Condition
     struct timeval  timer;
     int rcvtimeo=3000;
     //ex
     timer.tv_sec  =  rcvtimeo / 1000;
     if (setsockopt(c_sd, SOL_SOCKET, SO_RCVTIMEO,&timer, sizeof(timer)) < 0)
     perror("SO_RCVTIMEO setsockopt error");

     // Receive Data
     if ((numbytes = recv(c_sd, rcbuff, sizeof(rep_p.version), 0)) == -1)
     {
     perror("Receive - Timeout");
     exit(1);
     }

     // Get Version Number from Receive Buffer and Print on the screen
     memcpy(&val,rcbuff,sizeof(rep_p.version));
     cout<<"Success"<<endl;

     close(c_sd);

     return 0;*/
}

#if defined(__APPLE__)
ssize_t getline(char **linep, size_t *np, FILE *stream) {
    char *p = NULL;
    size_t i = 0;
    int ch = 0;

    if (!linep || !np) {
        errno = EINVAL;
        return -1;
    }

    if (!(*linep) || !(*np)) {
        *np = 120;
        *linep = (char *) malloc(*np);
        if (!(*linep)) {
            return -1;
        }
    }

    flockfile(stream);

    p = *linep;
    for (ch = 0; (ch = getc_unlocked(stream)) != EOF;) {
        if (i > *np) {
            /* Grow *linep. */
            size_t m = *np * 2;
            char *s = (char *) realloc(*linep, m);

            if (!s) {
                int error = errno;
                funlockfile(stream);
                errno = error;
                return -1;
            }

            *linep = s;
            *np = m;
        }

        p[i] = ch;
        if ('\n' == ch)
            break;
        i += 1;
    }
    funlockfile(stream);

    /* Null-terminate the string. */
    if (i > *np) {
        /* Grow *linep. */
        size_t m = *np * 2;
        char *s = (char *) realloc(*linep, m);

        if (!s) {
            return -1;
        }

        *linep = s;
        *np = m;
    }

    p[i + 1] = '\0';
    return ((i > 0) ? i : -1);
}
#endif
