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

#define BUFSIZE 8192 // buffersize and command size, CSIZE short for 1KB
#define CSIZE 1024

void handle_request(int connfd, char *path, int flag);
void handle_put(int connfd, char *path, int flag);
void handle_list(int connfd, char *path, int flag);
void handle_get(int connfd, char *path, int flag);

int listenfd;
/* Wrapper for perror */
void error(char *msg)
{
    fprintf(stderr, "%s", msg);
    exit(1);
}

void handle_list(int connfd, char *path, int flag)
{
    char ackbuf[16];
    char filename[1024];
    char cmd[64];
    bzero(&cmd, 64);
    bzero(&filename, 1024);
    bzero(&ackbuf, 16);

    recv(connfd, ackbuf, 4, 0);
    send(connfd, "+ACK", 4, 0);

    // list files in server directory
    // cut on the '-' delimiter and return the second and 3rd fields (filename and chunknumber)
    // sort the entries and only return the unique entries -> removes 2 files with same filename but different timestamps
    sprintf(cmd, "ls -t %s | cut -d'-' -f 2,3 | sort | uniq", path);
    FILE *ls = popen(cmd, "r");

    while (fgets(filename, 1024, ls) != NULL)
    {
        filename[strlen(filename) - 1] = '\0';
        send(connfd, filename, strlen(filename), 0);
        recv(connfd, ackbuf, 4, 0);
        bzero(&ackbuf, 4);
        bzero(&filename, 1024);
    }

    send(connfd, "FIN", 3, 0);
    handle_request(connfd, NULL, 0);
}

void handle_get(int connfd, char *path, int flag)
{
    char buf[BUFSIZE];
    char fullName[CSIZE + 16];
    char fname[CSIZE];
    char ls_cmd[CSIZE];
    char ackbuf[16];

    size_t received;
    bzero(&buf, BUFSIZE);
    bzero(&fname, CSIZE);
    bzero(&ackbuf, 16);
    bzero(&ls_cmd, CSIZE);
    // get file name and chunk number for client
    received = read(connfd, ls_cmd, CSIZE);
    // ls ./dfsn -> grep for filename requested by client -> sort list by timestamp & get file with last timestamp -> most recent
    sprintf(buf, "ls -t %s | grep %s | sort |tail -1", path, ls_cmd);
    FILE *ls = popen(buf, "r"); // open file w/ most recent timestamp
    bzero(&buf, BUFSIZE);
    bzero(&ls_cmd, CSIZE);

    if (fgets(fname, CSIZE, ls) == NULL) // did not find file
    {
        send(connfd, "-ACK", 4, 0);
    }
    else
    {
        fname[strlen(fname) - 1] = '\0';
        send(connfd, fname, strlen(fname), 0);     // send filename back to client as an acknowledgement
        sprintf(fullName, "./%s/%s", path, fname); // supply full path to file - looks like ./dfsN/filename instead of filename
        // open file and get number of bytes
        FILE *fp = fopen(fullName, "rb");
        fseek(fp, 0, SEEK_END);
        bzero(&buf, CSIZE);
        long numBytes = ftell(fp);
        size_t bytesRead;
        sprintf(buf, "%ld", numBytes);
        // send number of bytes in this chunk to client
        send(connfd, buf, strlen(buf), 0);
        read(connfd, ackbuf, 4);
        bzero(&buf, BUFSIZE);
        bzero(&ackbuf, 16);
        // return pointer to start of file
        fseek(fp, 0, SEEK_SET);
        // send BUFSIZE bytes at a time if the number of bytes we have left to read is >= BUFSIZE
        while (numBytes >= BUFSIZE)
        {
            bytesRead = fread(buf, 1, BUFSIZE, fp);
            send(connfd, buf, bytesRead, 0);
            read(connfd, ackbuf, 4);
            bzero(&buf, bytesRead);
            bzero(&ackbuf, 4);
            numBytes -= bytesRead;
        }
        // send remaining part of file - beej's ssendall routine pretty much
        while (numBytes > 0)
        {
            bytesRead = fread(buf, 1, numBytes, fp);
            send(connfd, buf, bytesRead, 0);
            read(connfd, ackbuf, 4);
            bzero(&buf, bytesRead);
            bzero(&ackbuf, 4);
            numBytes -= bytesRead;
        }
        // let client know that we are done
        send(connfd, "FIN", 3, 0);
        recv(connfd, ackbuf, 3, 0);
        fclose(fp);
    }
    fclose(ls);
    handle_request(connfd, path, flag);
}

void handle_put(int connfd, char *path, int flag)
{
    char buf[BUFSIZE];
    char fname[BUFSIZE];
    short chunkno;
    size_t received;

    bzero(&buf, BUFSIZE);
    read(connfd, buf, BUFSIZE);

    char *ptr = buf;
    char *temp = strsep(&ptr, " ");
    // temp now holds filename from client - store for later
    char *tempf = temp;
    // temp now holds chunk number
    temp = strsep(&ptr, " ");
    chunkno = atoi(temp);
    temp = strsep(&ptr, " ");
    char *timestamp = temp;
    // create full filename to store on server: serverDir/fileName-chunkNo
    sprintf(fname, "%s/%s-%s-%d", path, timestamp, tempf, chunkno);
    FILE *fp = fopen(fname, "wb");

    send(connfd, "ready", 5, 0);
    bzero(&buf, BUFSIZE);
    // simply write all bytes to a file until client tells us we are done.
    while ((received = read(connfd, buf, BUFSIZE)) > 0)
    {
        if (strcmp(buf, "PUT FIN ACK") == 0)
        {
            send(connfd, "PUT FIN ACK", 11, 0);
            break;
        }
        fwrite(buf, sizeof(char), received, fp);
        send(connfd, "+ACK", 4, 0);
        bzero(&buf, received);
    }
    // close file and return to recursive function
    fclose(fp);
    handle_request(connfd, path, 1);
}

// main loop until we run out of files / commands to go through from a client
void handle_request(int connfd, char *path, int flag)
{
    // printf("%d\n", flag);
    if (flag == -1)
    {
        printf("Closing connection\n");
        close(connfd);
        return;
    }

    char buf[16];
    bzero(&buf, 16);
    read(connfd, buf, 16);
    if (strcmp(buf, "GET") == 0)
    {
        send(connfd, "GET ACK", 7, 0);
        handle_get(connfd, path, flag);
    }
    else if (strcmp(buf, "PUT") == 0)
    {
        send(connfd, "PUT ACK", 7, 0);
        handle_put(connfd, path, flag);
    }
    else if (strcmp(buf, "LIST") == 0)
    {
        send(connfd, "LIST ACK", 8, 0);
        handle_list(connfd, path, flag);
    }
    else if (strcmp(buf, "-FIN"))
    {
        send(connfd, "FIN ACK", 7, 0);
        handle_request(connfd, NULL, -1);
    }
    else
    {
        handle_request(connfd, NULL, -1);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        error("Error: incorrect usage.\nProper command usage: ./dfs <dir_path> <portno>\n\n");
    }

    int portno, optval, connfd;
    struct sockaddr_in clientaddr;
    char *dirname = argv[1];
    mkdir(dirname, 0777);
    portno = atoi(argv[2]);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        error("ERROR creating socket");
    }

    bzero(&clientaddr, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    clientaddr.sin_port = htons(portno);

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
    while (1)
    {
        connfd = accept(listenfd, (struct sockaddr *)NULL, NULL);
        if (connfd == -1)
        {
            break;
        }
        if (fork() == 0)
        {
            close(listenfd);
            printf("Created child process for accepted connection\n");
            handle_request(connfd, dirname, 0);
            close(connfd);
        }
        close(connfd);
    }
    return 0;
}