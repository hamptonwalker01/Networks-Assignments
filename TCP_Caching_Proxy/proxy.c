#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <netdb.h>
#include <dirent.h>
#include <fcntl.h>
// #include <pthread.h>

#define BUFSIZE 8192
#define KB 1024
#define CACHE_PATH "./cache"
// basic structure of an HTTP request
struct Request
{
    char method[8];    // needs to be GET
    char uri[KB];      //
    char version[16];  // HTTP/1.0 or HTTP/1.1
    char host[2 * KB]; // not null if version == HTTP/1.1
    char host_and_file[3 * KB];
    char ip_addr[32];
    char contentType[64];
    char new_req[BUFSIZE];
    char req[BUFSIZE];
    int hash;
    int connfd;     //
    int portnumber; //
    int found80;
};

// Global variable for listening socket for server
// Close on Ctrl+C in handler function
int listenfd;

/* Global timeout value for cache */
int timeout;
/*
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

/*
 * Handler for graceful exit
 * close listening socket on Ctrl+C, not allowing any more requests
 */
void handler()
{
    close(listenfd);
}

/* Send error response to socket connfd with error code code
    Followed as described in the writeup */
void send_error(int connfd, int code, char *version)
{
    switch (code)
    {
    case 400:;
        char buf[128];
        sprintf(buf, "%s 400 Bad Request\r\n\r\nThe request could not be parsed or is malformed.", version);
        write(connfd, (char *)buf, strlen((char *)buf));
        break;

    case 403:;
        char buf2[128];
        sprintf(buf2, "%s 403 Forbidden\r\n\r\nThe requested file can not be accessed due to a file permission issue.", version);
        write(connfd, (char *)buf2, strlen((char *)buf2));
        break;

    case 404:;
        char buf3[128];
        sprintf(buf3, "%s 404 Not Found\r\n\r\nThe requested file can not be found in the document tree.", version);
        write(connfd, (char *)buf3, strlen((char *)buf3));
        break;

    case 405:;
        char buf4[128];
        sprintf(buf4, "%s 405 Method Not Allowed\r\n\r\nA method other than GET was requested.", version);
        write(connfd, (char *)buf4, strlen((char *)buf4));
        break;
    case 504:;
        char buf6[128];
        sprintf(buf6, "%s 504 Gateway Timeout\r\n\r\nThe server exceeded a timeout limit when trying to connect.", version);
        write(connfd, (char *)buf6, strlen((char *)buf4));
        break;
    case 505:;
        char buf5[128];
        sprintf(buf5, "%s 505 HTTP Version Not Supported\r\n\r\nAn HTTP version other than 1.0 or 1.1 was requested.", version);
        write(connfd, (char *)buf5, strlen((char *)buf5));
        break;

    default:
        printf("Unknown code: %d\n", code);
    }

    close(connfd);
    return;
}

/* https://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c */
int remove_directory(const char *path)
{
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;

    if (d)
    {
        struct dirent *p;
        r = 0;
        while (!r && (p = readdir(d)))
        {
            int r2 = -1;
            char *buf;
            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
                continue;
            size_t len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);

            if (buf)
            {
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path, p->d_name);
                if (!stat(buf, &statbuf))
                {
                    if (S_ISDIR(statbuf.st_mode))
                        r2 = remove_directory(buf);
                    else
                        r2 = unlink(buf);
                }
                free(buf);
            }
            r = r2;
        }
        closedir(d);
    }

    if (!r)
        r = rmdir(path);

    return r;
}

/* Helper function to get the length of an integer (base 10)*/
int get_int_len(int i)
{
    int count = 1;
    while (i > 1)
    {
        ++count;
        i /= 10;
    }
    return count;
}

/*
 * Parse request from client - i believe that this is completed
 */
int get_request(struct Request *req)
{
    int n;
    int foundDelim = 0;
    char buf[BUFSIZE];

    bzero(req->method, 2 * KB);
    bzero(req->host, 2 * KB);
    bzero(req->req, BUFSIZE);
    req->found80 = 0;
    // get first line of request and fill variables
    while ((n = read(req->connfd, buf, BUFSIZE)) > 0)
    {
        char tempbuf[BUFSIZE];
        strcpy(tempbuf, buf);
        char *delimSearcher = strstr(buf, "\r\n\r\n");
        if (strlen(req->method) == 0)
        {
            char *ptr = strstr(tempbuf, "GET");
            if (ptr)
            {
                char *token = strtok(ptr, " ");
                strcpy(req->method, token);
                token = strtok(NULL, " ");
                strcpy(req->uri, token);
                token = strtok(NULL, "\r\n");
                strcpy(req->version, token);
                // printf("method: %s\n", req->method);
                // printf("uri: %s\n", req->uri);
                // printf("version: %s\n", req->version);
            }
        }
        strcpy(tempbuf, buf);
        if (strlen(req->host) == 0)
        {
            char *temp = strstr(tempbuf, "Host: ");
            if (temp)
            {
                temp += 6;
                char *temp2 = strsep(&temp, "\r\n");
                strcpy(req->host, temp2);
                char temphost[strlen(req->host)];
                strcpy(temphost, req->host);
                strcpy(req->host_and_file, req->host);
                char *portptr = strrchr(temphost, ':');
                if (!portptr)
                {
                    req->portnumber = 80;
                }
                else
                { // how high was I when I wrote this
                    int size = (int)strlen(portptr);
                    for (size; size > 0; size--)
                    {
                        req->host[strlen(req->host) - 1] = '\0';
                    }
                    // strcpy(req->host_and_file, req->host);
                    portptr++;
                    req->portnumber = atoi(portptr);
                    if (req->portnumber == 80)
                    {
                        req->found80 = 1;
                    }
                }
            }
        }
        if (delimSearcher)
        {
            // printf("Found CLRF\n");
            strcat(req->req, buf);
            foundDelim = 1;
            bzero(buf, BUFSIZE);
            break;
        }
        strcat(req->req, buf);
        bzero(buf, BUFSIZE);
    }
    /* debug statements and stuff */
    // printf("\n\nmethod: %s\n", req->method);
    // printf("uri: %s\n", req->uri);
    // printf("version: %s\n", req->version);
    // printf("host: %s\n", req->host);
    // printf("portno: %d\n\n", req->portnumber);
    // printf("req->req: %s\n\n", req->req);

    // error checking
    if (!foundDelim)
    {
        // printf("No delim\n");
        send_error(req->connfd, 400, "HTTP/1.0");
        return 1;
    }

    if (req->method == NULL)
    {
        // printf("No method\n");
        send_error(req->connfd, 400, "HTTP/1.0");
        return 1;
    }

    if (!!strcmp(req->method, "GET"))
    {
        // printf("Method not get\n");
        send_error(req->connfd, 405, "HTTP/1.0");
        return 1;
    }
    if (req->uri == NULL)
    {
        // printf("No uri\n");
        send_error(req->connfd, 400, "HTTP/1.0");
        return 1;
    }
    if (!strcmp(req->version, "HTTP/1.1"))
    {
        // need to get host header
        if (strlen(req->host) > 0)
        {
            return 0;
        }
        else
        {
            // printf("Send a 400 error, HOST not found\n");
            send_error(req->connfd, 400, req->version);
            return 1;
        }
    }
    else if (!strcmp(req->version, "HTTP/1.0"))
    {
        return 0;
    }
    else if (!strlen(req->version))
    {
        // printf("No HTTP version found in request.\n");
        send_error(req->connfd, 400, "HTTP/1.0");
        return 1;
    }
    else
    {
        // printf("Unsupported HTTP version\n");
        send_error(req->connfd, 505, "HTTP/0.9");
        return 1;
    }
    return 0;
}

/* Hash Function: stolen from geeksforgeeks
 * https://www.geeksforgeeks.org/string-hashing-using-polynomial-rolling-hash-function/
 * @PARAMS
 * const char *s -> string to be hashed
 * const int n -> length of string *s
 */
int get_hash(const char *s, const int n)
{
    const int p = 11111, m = 1e9 + 9;
    int hash = 0;
    long p_pow = 1;
    for (int i = 0; i < n; i++)
    {
        hash = (hash + (s[i] - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return hash;
}

/* Helper function to establish connection to host specified by client */
int get_connection(struct Request *req)
{
    // printf("\nTrying to create socket to connect to requested server...\n");
    struct sockaddr_in serveraddr;
    int optval = 1;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("ERROR creating socket to remote\n");
    }
    // printf("Portno: %d\n", req->portnumber);
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(req->ip_addr);
    serveraddr.sin_port = htons(req->portnumber);
    /* reuse addr enabled for debugging purposes */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
    {
        printf("ERROR setting sockopt SO_REUSEADDR\n");
        return -1;
    }
    /* Setting timeout */
    struct timeval timeout;
    timeout.tv_usec = 0;
    timeout.tv_sec = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) < 0)
    {
        error("ERROR setting sockopt TIMEOUT\n");
    }

    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        printf("ERROR connecting\n");
        return -2;
    }

    return sockfd;
}

/* Function from PA2 */
void set_content_type(char *uri, struct Request *req)
{
    /* Define path: ./www directory + uri from response */
    char *path = (char *)malloc(BUFSIZE);
    bzero(path, BUFSIZE);
    strcpy(path, uri);

    //  get last instance of "." -> content type of file
    char *c_type = strrchr(uri, '.');
    char end = uri[strlen(uri) - 1];
    if (c_type == NULL) // directory
    {
        if (end == '/')
        {
            // printf("We have a directory\n");
            strcat(path, "index.html");
            strcpy(req->contentType, "Content-Type: text/html\r\n");
            // strcpy(req->filename, path);
        }
        else
        {
            // 404 file not found -> foo/bar is definitely not a file I can support
            send_error(req->connfd, 404, req->version);
        }
    }
    /* remove period from pointer to path
        i.e. get substring */
    else
    {
        // copy contents of pointer into malloc'ed region, increment pointer by 1 -> now pointing to extension after the .
        char *true_type = (char *)malloc(16);
        strcpy(true_type, c_type);
        true_type++;
        // printf("\n\n%s\n\n", true_type);

        /* check if valid content type*/
        // text content types
        if (!strcmp(c_type, ".html") || !strcmp(c_type, ".css") || !strcmp(c_type, ".htm"))
        {
            sprintf(req->contentType, "Content-Type: text/%s\r\n", true_type);
        }
        else if (!strcmp(c_type, ".txt"))
        {
            strcpy(req->contentType, "Content-Type: text/plain\r\n");
        }
        // image content types
        else if (!strcmp(c_type, ".png") || !strcmp(c_type, ".gif") || !strcmp(c_type, ".jpg"))
        {
            sprintf(req->contentType, "Content-Type: image/%s\r\n", true_type);
        }
        // application content type
        else if (!strcmp(c_type, ".js") || !strcmp(c_type, ".pdf"))
        {
            sprintf(req->contentType, "Content-Type: application/%s\r\n", true_type);
        }
        else if (!strcmp(c_type, ".ico"))
        {
            strcpy(req->contentType, "Content-Type: image/x-icon\r\n");
        }
        else if (!strcmp(c_type, ".mp4"))
        {
            strcpy(req->contentType, "Content-Type: video/mp4\r\n");
        }
        else // UNSUPORTED CONTENT TYPE
        {
            // free malloc'ed memory before sending error and return
            true_type--;
            free(true_type);
            send_error(req->connfd, 400, req->version);
            return;
        }
        // free malloc'ed memory
        true_type--;
        free(true_type);
    };
    free(path);
    return;
}

/* Helper function to extract filename from URI */
int extract_filename(struct Request *req, int flag)
{
    char temp[BUFSIZE];
    strcpy(temp, req->req);

    char *ptr = strstr(temp, req->host);
    ptr += strlen(req->host);

    if (req->portnumber != 80 || (req->portnumber == 80 && req->found80))
    {
        ptr += get_int_len(req->portnumber);
    }
    // print GET path (rest of headers) to new_req

    char *path = strtok(ptr, " ");
    size_t m_size = strlen(path) + strlen(req->host) + 2;
    sprintf(req->new_req, "GET %s %s\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
            path, req->version, req->host, req->portnumber);
    // printf("New Req: %s\n", req->new_req);
    char *host_and_file = (char *)malloc(m_size);
    strcpy(host_and_file, req->host);
    strcat(host_and_file, path);
    // printf("Host and file %s\tport: %d\n", host_and_file, req->portnumber);
    if (!flag)
    {
        printf("NOT DYNAMIC CONTENT\n");
        req->hash = get_hash(host_and_file, (int)strlen(host_and_file));
        set_content_type(path, req);
        printf("Computed hash: %d\n", req->hash);
        printf("%s\n", req->contentType);
    }
    free(host_and_file);

    // clear req->req
    bzero(req->req, BUFSIZE);
    return 0;
}

/* Wrapper for gethostbyname, also gets ip address from hostname */
int hostname_check(struct Request *req)
{
    struct hostent *temp = gethostbyname(req->host);
    if (temp == NULL)
    {
        return 1;
    }
    strcpy(req->ip_addr, inet_ntoa(*(struct in_addr *)temp->h_addr));
    printf("Got ip addr: %s\n", req->ip_addr);
    return 0;
}

/* Function to check blocklist file for host/ipaddr entry */
int blocklist_check(struct Request *req)
{
    /* open the blocklist file for reading*/
    FILE *fp = fopen("blocklist", "rb");
    size_t len;
    ssize_t read;
    char *line = NULL;
    /* loop to read blocklist file by line until we get to the end of the file or encounter a */
    while ((read = getline(&line, &len, fp)) > 0)
    { // compare the line read with the req's host and IP addr, up to the newline character
        // if true -> host/ip in blocklist -> alert and send error
        if ((strncmp(line, req->host, (size_t)read - 1) == 0) || (strncmp(line, req->ip_addr, (size_t)read - 1) == 0))
        {
            fclose(fp);
            return 1;
        }
    }

    if (line)
        free(line);

    fclose(fp);
    return 0;
}

int check_timeout(char *filename)
{
    struct stat filestat;
    stat(filename, &filestat);
    int diff = (int)difftime(time(NULL), filestat.st_mtim.tv_sec);
    // printf("Time difference : %d\n", diff);
    // printf("Timeout : %d\n", timeout);
    if (diff >= timeout)
    {
        // delete file from cache
        printf("Removing file from cache!\n");
        remove(filename);
        return 1;
    }
    return 0;
}

int get_cached_file(char *filename, struct Request *req)
{
    if (check_timeout(filename))
        return 1;

    ssize_t n, sent;
    struct flock lock;
    char statusMsg[32];
    char contentLength[64];
    char connection[32];
    char headers[256];

    FILE *fp = fopen(filename, "rb");
    // memset(&lock, 0, sizeof(lock));
    // lock.l_type = F_RDLCK;
    // printf("Setting READ lock on file...\n");
    // fcntl(fileno(fp), F_SETLK, &lock);
    fseek(fp, 0, SEEK_END);
    /* Setting content length and status headers */
    sprintf(contentLength, "Content-Length: %ld\r\n", ftell(fp));
    sprintf(statusMsg, "%s 200 OK\r\n", req->version);
    strcpy(connection, "Connection: close\r\n\r\n");

    fseek(fp, 0, SEEK_SET);
    sprintf((char *)headers, "%s%s%s%s", statusMsg, req->contentType, contentLength, connection);

    n = write(req->connfd, headers, strlen(headers));
    if (n < 0)
    {
        fclose(fp);
        error("WRITE ERROR");
    }
    // now, read file contents and write at the same time
    while ((n = fread(req->req, 1, BUFSIZE, fp)) > 0)
    {
        sent = write(req->connfd, req->req, n);
        if (sent < 0)
        {
            printf("SENT < 0 on write\n");
            fclose(fp);
            return -1;
        }
        bzero(req->req, BUFSIZE);
    }
    // close file
    // printf("unlocing READ loc\n");
    // lock.l_type = F_UNLCK;
    // fcntl(fileno(fp), F_SETLK, &lock);
    fclose(fp);
    return 0;
}
/* Write client response to server and send */
void send_and_receive(struct Request *req, int sockfd, char *filename, int flag)
{
    // sockfd holds socket to server
    // req->connfd
    int contentLength;
    FILE *fp;
    // struct flock lock;

    if (!flag)
    {
        fp = fopen(filename, "wb");
        if (!fp)
        {
            printf("Uh oh, can't open file\n");
            exit(1);
        }

        // memset(&lock, 0, sizeof(lock));
        // lock.l_type = F_WRLCK;
        // printf("Setting WRITE lock on file...\n");
        // fcntl(fileno(fp), F_SETLKW, &lock);
    }
    ssize_t n, sent, recvd;
    n = send(sockfd, req->new_req, strlen(req->new_req), 0);
    if (n < 0)
    {
        exit(1);
    }
    // printf("Bytes sent: %ld\nLength of message: %ld\n", n, strlen(req->new_req));
    int delimCheck = 0;
    bzero(req->req, BUFSIZE);
    // send the request to the server sockfd with a write
    // read response from server and write back to client socket

    while ((recvd = recv(sockfd, req->req, BUFSIZE, 0)) > 0)
    {
        // printf("Received %ld bytes from server\n", recvd);
        if (!delimCheck)
        {
            char tempreq[recvd];
            bzero(tempreq, recvd);
            strncpy(tempreq, req->req, recvd);
            char *clength = strstr(tempreq, "Content-Length");
            char *p = strstr(req->req, "\r\n\r\n");
            if (clength)
            {

                clength = strtok(clength, " ");
                clength = strtok(NULL, "\r\n");
                contentLength = atoi(clength);
                // printf("Content Length: %d\n", contentLength);
            }
            if (p)
            {
                p += 4;
                // write to file in cache
                int pos = p - req->req;
                size_t bytes_left = (size_t)recvd - (size_t)pos;

                if (bytes_left > 0 && !flag)
                {
                    fwrite(p, sizeof(char), bytes_left, fp);
                }
                delimCheck = 1;
            }
        }
        else if (!flag)
        { // else if not dynamic content, write all
            fwrite(req->req, sizeof(char), (size_t)recvd, fp);
        }

        // write to client and clear buffer
        // n = fwrite(req->req, sizeof(char), recvd, dummy);
        sent = send(req->connfd, req->req, recvd, 0);
        // printf("Sent %ld bytes to client\n", sent);
        bzero(req->req, BUFSIZE);
    }
    // if (fp)
    // {
    //     printf("unlocking WRITE lock\n");
    //     /* Release the lock. */
    //     lock.l_type = F_UNLCK;
    //     fcntl(fileno(fp), F_SETLKW, &lock);
    // }
    fclose(fp);
    return;
}

/* Used to be a larger wrapper function. Just calls set_content _type -> send_content upon all successes
 On errors, you will find send_error calls within set_content_type or send_content to send appropriate error to client*/
void build_response(struct Request *req)
{
    // printf("Trying to build response...\n");
    /* verify that hostname maps to an ip address
        if not, send an error */
    if (hostname_check(req))
    {
        // cannot resolve hostname
        send_error(req->connfd, 404, req->version);
        return;
    }
    else if (blocklist_check(req))
    { // hostname/ip address in blacklist file, send back an error
        send_error(req->connfd, 403, req->version);
        return;
    }
    else
    {
        // response is all good
        /* get just the filename for the request */
        int dynamicFlag = (strstr(req->uri, "?") == NULL) ? 0 : 1;
        // printf("Dynamic flag: %d\n", dynamicFlag);
        extract_filename(req, dynamicFlag);
        char filename[get_int_len(req->hash) + 10];
        sprintf(filename, "%s/%d", CACHE_PATH, req->hash);

        if (!dynamicFlag && access(filename, F_OK) == 0)
        {
            if (get_cached_file(filename, req) == 0)
            {
                printf("SENT FILE FROM CACHE\n");
                return;
            }
        }
        int servfd = get_connection(req);
        if (servfd == -2)
        {
            printf("SENDING 504 ERROR");
            send_error(req->connfd, 504, req->version);
            return;
        }
        else if (servfd < 0)
        {
            send_error(req->connfd, 400, req->version);
            return;
        }

        send_and_receive(req, servfd, (char *)filename, dynamicFlag);
        close(servfd);
    }

    // printf("\nres->contentType: %s\n", res->contentType);
    return;
}

int main(int argc, char **argv)
{
    int connfd, optval, portno;
    struct sockaddr_in clientaddr;
    /* When receiving an interrupt from user, call handler -> close listen socket
     * This is what causes a graceful exit */
    signal(SIGINT, handler);
    mkdir(CACHE_PATH, 0770);
    /* Check arguments supplied correctly: filename portnum*/
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <port:int> <timeout:int>\n", argv[0]);
        exit(1);
    }
    // set portnum and timeout
    portno = atoi(argv[1]);
    timeout = atoi(argv[2]);
    // try to create a socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        error("ERROR creating socket\n");
    }

    // set up address
    bzero(&clientaddr, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    clientaddr.sin_port = htons(portno);

    /* Set sockopts to allow us to rerun our server immediately after killing
     * useful for debugging purposes
     */
    optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    // try binding
    if ((bind(listenfd, (struct sockaddr *)&clientaddr, sizeof(clientaddr))) < 0)
    {
        error("ERROR binding");
    }
    // start listening for up to 25 active connections
    if ((listen(listenfd, 25)) < 0)
    {
        error("ERROR listening");
    }
    printf("Server listening for connections on port %d\n", portno);
    // begin infinite loop
    // runs until we encounter an error accepting or encounter a Ctrl+C -> call handler to close listening socket -> next accept returns -1
    while (1)
    {

        connfd = accept(listenfd, (struct sockaddr *)NULL, NULL);
        if (connfd == -1)
        {
            break;
        }
        // printf("Received connection from client...\n");

        /* Child proccess to handle request */
        if (fork() == 0)
        {
            close(listenfd); // close parent socket
            /* build request structure*/
            struct Request request;
            request.connfd = connfd;

            /* Get the HTTP Request from the client
             * If Zero, HTTP Request is syntactically correct
             * Call Build Response to check content type and service request/handle errors
             * Else, print debug statement saying that there was an error in the HTTP request
             */
            (get_request(&request) == 0) ? build_response(&request) : printf("uh oh somebody provided a bad request :(\n");
            close(connfd); // close connection and exit
            exit(0);
        }
        // parent: close child socket
        close(connfd);
    }
    // once we break out of the loop, exit with status 0
    remove_directory(CACHE_PATH);
    exit(0);
}