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

#define BUFSIZE 8192
// basic structure of an HTTP request
struct Request
{
    char *method;  // needs to be GET
    char *uri;     //
    char *version; // HTTP/1.0 or HTTP/1.1
    char *host;    // not null if version == HTTP/1.1
    int connfd;    //
};

// http response to be sent to client
struct Response
{
    char statusMsg[256];
    char contentType[64];
    char contentLength[256];
    char filename[BUFSIZE];
    int connfd;
};

// Global variable for listening socket for server
// Close on Ctrl+C in handler function
int listenfd;

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
void send_error(int connfd, int code)
{
    switch (code)
    {
    case 400: ;
        char buf[64] = "HTTP/1.0 400 Bad Request\r\n\r\n";
        write(connfd, (char *)buf, strlen((char *)buf));
        break;

    case 403: ;
        char buf2[64] = "HTTP/1.0 403 Forbidden\r\n\r\n";
        write(connfd, (char *)buf2, strlen((char *)buf2));
        break;

    case 404: ;
        char buf3[64]= "HTTP/1.0 404 Not Found\r\n\r\n";
        write(connfd, (char *)buf3, strlen((char *)buf3));
        break;

    case 405: ;
        char buf4[64] = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
        write(connfd, (char *)buf4, strlen((char *)buf4));
        break;

    case 505: ;
        char buf5[64] = "HTTP/1.0 505 HTTP Version Not Supported\r\n\r\n";
        write(connfd, (char *)buf5, strlen((char *)buf5));
        break;

    default:
        printf("Unknown code: %d\n", code);
    }

    close(connfd);
    return;
}

/*
 * Parse request from client
 */
int get_request(struct Request *req)
{
    int n;
    char buf[BUFSIZE];
    // get first line of request and fill variables
    n = read(req->connfd, buf, BUFSIZE);
    char *bufptr = buf;

    /* basic error checking*/
    if (n < 0)
    {
        error("ERROR in read in get_request");
    }
    else if (n == 0)
    {
        // printf("EMPTY REQUEST\n");
        send_error(req->connfd, 400);
        return 1;
    }
    // make sure request contains \r\n\r\n and ends in \r\n\r\n
    char *delimSearcher = strstr(buf, "\r\n\r\n");
    if (!delimSearcher || strlen(delimSearcher) != 4)
    {
        send_error(req->connfd, 400);
        return 1;
    }

    // get method of request
    req->method = strsep(&bufptr, " ");
    // make sure pointers exists, if not 400 error
    if (req->method == NULL || bufptr == NULL)
    {
        send_error(req->connfd, 400);
        return 1;
    }
    if (!!strcmp(req->method, "GET"))
    {
        send_error(req->connfd, 405);
        return 1;
    }
    // get uri of request
    req->uri = strsep(&bufptr, " ");
    if (req->uri == NULL || bufptr == NULL)
    {
        send_error(req->connfd, 400);
        return 1;
    }

    // get version of request
    req->version = strsep(&bufptr, "\r\n");
    if (req->version == NULL)
    {
        error("ERROR trying to call strsep to get request version");
    }

    printf("VERSION: %s\n", req->version);
    // printf("%ld\n", strlen(req->version));
    // check for host header if version is HTTP/1.1
    if (!strcmp(req->version, "HTTP/1.1"))
    {
        // need to get host header
        char *hostCheck = strstr(bufptr, "Host: ");
        if (hostCheck)
        {
            req->host = strsep(&hostCheck, "\r\n");
            // printf("req->host: %s\n", req->host);
            return 0;
        }
        else
        {
            req->host = NULL;
            // printf("Send a 400 error, HOST not found\n");
            send_error(req->connfd, 400);
            return 1;
        }
    }
    else if (!strcmp(req->version, "HTTP/1.0"))
    {
        return 0;
    }
    else if (!strlen(req->version))
    {
        // printf("No HTTP version found in request.");
        send_error(req->connfd, 400);
        return 1;
    }
    else
    {
        // unsupported HTTP Version
        // printf("Unsupported HTTP Version error\n");
        send_error(req->connfd, 505);
        return 1;
    }
}

void send_content(char *version, struct Response *res)
{
    char headers[1024];
    char fileBuf[BUFSIZE];
    ssize_t n, sent;

    /* Check if file exists */
    if (access(res->filename, F_OK) != 0)
    {
        // file does not exist 404 error
        send_error(res->connfd, 404);
        return;
    }
    /* Make sure file is readable */
    if (access(res->filename, R_OK) != 0)
    {
        // send 403 error
        send_error(res->connfd, 403);
        return;
    }
    // printf("Trying to open file...\n");
    // file exists and is readable if we are here
    // open file and copy length into res->contentLength
    FILE *fp = fopen(res->filename, "rb");
    // printf("Trying to seek...\n");
    fseek(fp, 0, SEEK_END);
    // printf("file size: %ld\n", ftell(fp));
    /* Setting content length and status headers */
    sprintf(res->contentLength, "Content-Length: %ld\r\n\r\n", ftell(fp));
    sprintf(res->statusMsg, "%s 200 OK\r\n", version);

    fseek(fp, 0, SEEK_SET); // reset file pointer
    // printf("Status message: %s\n", res->statusMsg);
    // set headers and prepare for content
    sprintf((char *)headers, "%s%s%s", res->statusMsg, res->contentType, res->contentLength);
    printf("\nheaders:\n%s", headers); // print for debugging
    // send headers to client
    n = write(res->connfd, headers, strlen(headers));
    if (n < 0)
    {
        fclose(fp);
        error("WRITE ERROR");
    }
    // now, read file contents and write at the same time
    while ((n = fread(fileBuf, 1, BUFSIZE, fp)) > 0)
    {
        // printf("Buf: %s\n", fileBuf);
        sent = write(res->connfd, fileBuf, n);
        if (sent < 0)
        {
            printf("SENT < 0 on write\n");
            // send_error(res->connfd);
            fclose(fp);
            return;
        }
        memset(fileBuf, 0, BUFSIZE);
        // printf("sent: %ld\tn: %ld\n", sent, n);
    }
    // close file
    fclose(fp);
}

/*
    List of valid content types:
     * text/content_type -> .html .txt .css
     * image/content_type -> .png .gif .jpg
     * application/javascript -> .js

    Approach: search for last '.', get string from '.' to '\0' -> content type
    Verify content type is acceptable
*/
void set_content_type(char *uri, struct Response *res, char *version)
{
    /* Define path: ./www directory + uri from response */
    char *path = (char *)malloc(BUFSIZE);
    bzero(path, BUFSIZE);
    sprintf(path, "%s%s", "./www", uri);

    /* check for 403 error: user tries to go back a directory
     * note: when testing within browser, this never triggers when suppling '../' within the path after the port
     */
    char *temp;
    if ((temp = strstr(path, "../")) != NULL)
    {
        send_error(res->connfd, 403);
        return;
    }

    //  get last instance of "." -> content type of file
    char *c_type = strrchr(uri, '.');
    char end = uri[strlen(uri) - 1];
    // printf("End: %c\n", end);
    if (c_type == NULL) // directory
    {
        if (end == '/')
        {
            // printf("We have a directory\n");
            strcat(path, "index.html");
            strcpy(res->contentType, "Content-Type: text/html\r\n");
            strcpy(res->filename, path);
        }
        else
        {
            // 404 file not found -> foo/bar is definitely not a file I can support
            send_error(res->connfd, 404);
            return;
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
        if (!strcmp(c_type, ".html") || !strcmp(c_type, ".css"))
        {
            sprintf(res->contentType, "Content-Type: text/%s\r\n", true_type);
        }
        else if (!strcmp(c_type, ".txt"))
        {
            strcpy(res->contentType, "Content-Type: text/plain\r\n");
        }
        // image content types
        else if (!strcmp(c_type, ".png") || !strcmp(c_type, ".gif") || !strcmp(c_type, ".jpg"))
        {
            sprintf(res->contentType, "Content-Type: image/%s\r\n", true_type);
        }
        // application content type
        else if (!strcmp(c_type, ".js"))
        {
            strcpy(res->contentType, "Content-Type: application/javascript\r\n");
        }
        else if (!strcmp(c_type, ".ico")) {
            strcpy(res->contentType, "Content-Type: image/x-icon\r\n");
        }
        else // UNSUPORTED CONTENT TYPE
        {
            // free malloc'ed memory before sending error and return
            true_type--;
            free(true_type);
            send_error(res->connfd, 400);
            return;
        }
        // free malloc'ed memory
        true_type--;
        free(true_type);

        // valid content type, check if file exists in next function
        strcpy(res->filename, path);
    }
    free(path);
    // printf("\nTrying to send content\n");
    // call send_content to check if file exists and then transfer file to client
    send_content(version, res);
    // printf("\n\nSUCCESSFUL\n\n");
}

/* Used to be a larger wrapper function. Just calls set_content _type -> send_content upon all successes
 On errors, you will find send_error calls within set_content_type or send_content to send appropriate error to client*/
void build_response(struct Request *req, struct Response *res)
{
    // printf("Trying to get content type...\n");
    set_content_type(req->uri, res, req->version);
    // printf("\nres->contentType: %s\n", res->contentType);
    return;
}

int main(int argc, char **argv)
{
    int connfd, optval, portno;
    struct sockaddr_in servaddr;
    /* When receiving an interrupt from user, call handler -> close listen socket
     * This is what causes a graceful exit */
    signal(SIGINT, handler);
    /* Check arguments supplied correctly: filename portnum*/
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    // set portnum
    portno = atoi(argv[1]);

    // try to create a socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        error("ERROR creating socket\n");
    }

    // set up address
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(portno);

    /* Set sockopts to allow us to rerun our server immediately after killing
     * useful for debugging purposes
     */
    optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    // try binding
    if ((bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0)
    {
        error("ERROR binding");
    }
    // start listening for up to 10 active connections
    if ((listen(listenfd, 10)) < 0)
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
            /* build request and response structures */
            struct Request request;
            struct Response response;
            request.connfd = connfd;
            response.connfd = connfd;

            /* Get the HTTP Request from the client
             * If Zero, HTTP Request is syntactically correct
             * Call Build Response to check content type and service request/handle errors
             * Else, print debug statement saying that there was an error in the HTTP request
             */
            (get_request(&request) == 0) ? build_response(&request, &response) : printf("uh oh somebody provided a bad request :(\n");
            close(connfd); // close connection and exit
            exit(0);
        }
        // parent: close child socket
        close(connfd);
    }
    // once we break out of the loop, exit with status 0
    exit(0);
}