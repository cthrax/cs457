#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#if defined(__APPLE__)
#include <stdio.h>   /* flockfile, getc_unlocked, funlockfile */
#include <stdlib.h>  /* malloc, realloc */

#include <errno.h>   /* errno */
#include <unistd.h>  /* ssize_t */

ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif

#include "awget.h"
// Default filename, if no filename is found in URL
const char *DEFAULT_FILENAME = "index.html";
const char *DEFAULT_CHAINFILE = "chaingang.txt";

int main(int argc, char *argv[]) {
    int in_opt = 0;         // input of getopt
    char* url = NULL;       // url buffer
    char *chfile = NULL;    // Chain file name

    // Reading from Chain file
    FILE *fp = NULL;        // File pointer for chain file
    char *ip_addr = NULL;   // char ip address
    char *line = NULL;      // to read a line from chain file using getline
    size_t line_len = 25;   // line length for malloc() *line
    char *tok = NULL;       // For tokenising line

    int port_no = 0; // Port Number got from chain file

    char ssbuff[512] = ""; // Buffer to be sent to SS
    int mem_offset = 0;

    int arg_counter = 1;

    //Structure Intialization
    struct ss_packet verss;

    // Remove URL
    for (; arg_counter < argc; arg_counter++) {
        // If we've found an option, we don't care.
        if (argv[arg_counter][0] == '-') {
            arg_counter++;
            continue;

        } else {
            // We found an input without an option, remove it from argv
            // and store to see if it is the url.
            url = argv[arg_counter];
            argc--;
            while (arg_counter < argc) {
                argv[arg_counter] = argv[++arg_counter];
            }
            break;
        }
    }

    // Socket Creation
    int cli_sd = 0; // Socket descriptor to SS
    int rv = 0;
    struct addrinfo hints, *server_sck_addr, *p;
    int rand_ss; // to select a random SS

    int numbytes; // Sending req. to SS
    char sizebuff[10] = ""; // Charr[] to receiving file size
    long int fisize = 0; // File Size
    char *databuff; // Receiving File Data
    int bytes = 0; // Data bytes received

    // Finding Filename from URL, Writing Data got from SS into File
    const char *filename; // URL File name
    char *f_ptr; // ptr to filename
    int bytes_write = 0; // Bytes written into file
    int file_fd; // File descriptor for opening file
    char write_buf[4096] = "";// Buffer for writing into file
    int offset = 0; // Data buffer offset
    int chunk = 4096; // Chunk size to be written
    int count = 0, i = 0;

    //getopt() function to do SANIY CHECK FOR INPUTS
    while ((in_opt = getopt(argc, argv, "c:")) != -1) {
        switch (in_opt) {
        case 'c':
            //Getting Chain file Name
            chfile = optarg;
            break;

        case '?':
            printf("Optarg: %s", optarg);
            break;

        default:
            printf("Default\n");
            abort();
        }
    }

    if (optind < argc) {
        fprintf(stderr,
                "Invalid arguments found. Format is 'awget <URL> [-c chainfile]'.\n");
        return -1;
    }

    verss.version = 1;
    verss.url = url;

    if (chfile == NULL) {
        chfile = DEFAULT_CHAINFILE;
    }

    printf("Opening %s\n", chfile);
    // Opening a Chain File pointed
    if ((fp = fopen(chfile, "r")) == NULL) {
        perror("fopen");
        exit(1);
    }

    //We limit hops to 256, so three digits is enough.(plus \0)
    char hops[4];
    for (i=0; i < 3; i++) {
        char c = fgetc(fp);
        hops[i] = c;
        if (c == '\0') {
            break;
        }
    }

    if (hops[i+1] != '\0') {
        fprintf(stderr, "Invalid chainfile, check number of hops.(longer than 255).\n");
        exit(1);
    }

    verss.step_count = atoi(hops);
    if (verss.step_count == 0) {
        fprintf(stderr, "Invalid chainfile, check number of hops. (atoi)\n");
        exit(1);
    }

    verss.steps = (struct int_tuple*) malloc(sizeof(struct int_tuple)*verss.step_count);

    //  Getting IP,Port Tuple for each SS
    while ((getline(&line, &line_len, fp) > 0)) {
        struct int_tuple* cur = (verss.steps + count);
        tok = strtok(line, " ");
        struct sockaddr_in temp;
        inet_pton(AF_INET, tok, &(temp.sin_addr));
        cur->ip_addr = htonl(temp.sin_addr.s_addr);

        tok = strtok(NULL, "\n");
        cur->port_no = htons(atoi(tok));

        count++;
    }

    if (count == 0 || count != verss.step_count) {
        fprintf(stderr, "Invalid chainfile, hop count doesn't match address acount.\n");
        exit(1);
    }

    fclose(fp);

    printf("No.of SS : %d\n", verss.step_count);
    printf("Request:%s \n", url);

    mem_offset = sizeof(struct ss_packet) + (sizeof(struct int_tuple) * verss.step_count);
    //XXX: remove.
    printf("Packet size: %d\n", mem_offset);

    // Create a Socket
    if ((cli_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket");
        exit(1);
    }

    printf("chainlist is \n");


    // picking a random ss from the list
    srand(time(NULL));
    rand_ss = rand() % verss.step_count;

    struct int_tuple* nextSS;
    char nextIp[INET_ADDRSTRLEN];
    uint16_t nextPort = 0;

    // Printing the chainlist
    for (i = 0; i < verss.step_count; i++) {
        struct int_tuple* cur = (verss.steps + i);
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

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char sPort[6];
    sprintf(sPort, "%d", nextPort);
    if ((rv = getaddrinfo(nextIp, sPort, &hints, &server_sck_addr)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = server_sck_addr; p != NULL; p = p->ai_next) {
        if ((cli_sd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(cli_sd, p->ai_addr, p->ai_addrlen) == -1) {
            close(cli_sd);
            perror("client: connect");
            continue;
        }

        break;
    }

    printf("\nSending data... \n");

    // Send Data to SS
    if ((numbytes = send(cli_sd, &verss, mem_offset, 0)) == -1) {
        close(cli_sd);
        perror("URL Send");
        exit(1);
    }

    printf("\nAwaiting Response from SS...\n");

    // Read the file size from the Client
    if (recv(cli_sd, sizebuff, 10, 0) < 0) {
        close(cli_sd);
        perror("File Size Receive");
        exit(1);
    }

    printf("\nReceived File Size\n");

    // Convert file size to host form from network using ntohl()
    memcpy(&fisize, sizebuff, sizeof(fisize));
    fisize = ntohl(fisize);
    printf("Filesize = %ld\n", fisize);
    if (fisize > 0) {
        // malloc() buffer to receive file data using file size
        databuff = (char*) malloc(fisize);

        // Read the File Data from the Client
        if ((bytes = recv(cli_sd, databuff, fisize, MSG_WAITALL)) < 0) {
            close(cli_sd);
            perror("File Data Receive");
            exit(1);
        }

        printf("\nReceived File Data\n");

        // Close the socket with SS
        close(cli_sd);

        // Sleep to allow file deletion on Last Hop[If not done then file on awget got deleted due to same filesystem for awget and SS]
        sleep(1);

        // Getting File Name from URL
        f_ptr = strrchr(url, '/');
        if (f_ptr == NULL) {
            filename = (char *) DEFAULT_FILENAME;
        } else {
            f_ptr++;
            filename = (char *) f_ptr;
        }
        printf("Filename = %s\n", filename);

        if ((file_fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0666))
                == -1) {
            close(cli_sd);
            perror("Open file");
            exit(1);
        }
        // Writing the file data into file
        while (fisize > 0) {
            if (fisize > chunk) {
                memcpy(&write_buf, databuff+offset, chunk);
                fisize -= chunk;
                offset += chunk;

                bytes_write = write(file_fd, &write_buf, chunk);
                if (bytes_write <= 0) {
                    close(cli_sd);
                    perror("File Write");
                    exit(1);
                }
            } else {
                memcpy(&write_buf, databuff+offset, fisize);

                bytes_write = write(file_fd, &write_buf, fisize);
                if (bytes_write <= 0) {
                    close(cli_sd);
                    perror("File Write");
                    exit(1);
                }
                fisize -= fisize;
            }
        }
        close(file_fd);
        printf("\nFile Received... Goodbye!\n\n");
    } else {
        printf("\nFile Size is zero, File not received, Quitting...\n\n");
    }
    close(cli_sd);
    return 0;
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

