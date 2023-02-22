#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/time.h>

#define BUFSIZE 8192
#define CSIZE 1024
// global variables
int sockfd[4];                  // list of 4 socket descriptors for 3 servers
struct sockaddr_in servers[4];  // for connecting to servers
short status[4] = {0, 0, 0, 0}; // status of servers -> 0 means dead, 1 means alive
int connections = 0;            // number of connections to servers
int lines = 4;                  // max lines in the config file / max number of connections

struct chunk
{
    char chunks[4];
    int chunkNum;
};
struct chunk chunkList[4];

struct fileCount
{
    char fname[256];
    int check;
};

/* Define function names here for ease of reference / no implicit declarations */
void addPair(char *pair, int idx, struct chunk *c);
void error(char *msg);
void read_conf();
unsigned long hash(unsigned char *str);
void handle_list();
void handle_get(char **files, int numFiles, int flag);
void handle_put(char **files, int numFiles, int flag);
void _close();

// adds chunk pair to chunk array for puts
void addPair(char *pair, int idx, struct chunk *c)
{
    bzero(c, sizeof(c));
    c->chunkNum = idx + 1;
    strcpy(c->chunks, pair);
}

/* Wrapper for perror */
void error(char *msg)
{
    fprintf(stderr, "%s", msg);
    exit(1);
}

// helper function to incrementally add chunks - made from staring at example table in project writeup for about an hour
void increment(int *idx, int flag)
{
    int temp = *idx;
    if (flag)
        temp++;
    if (temp == 5)
        temp = 1;
    while (status[temp - 1] != 1)
    {
        // printf("Server %d dead\n", temp);
        temp++;
        if (temp > 4)
            temp = 1;
    }
    *idx = temp;
}

// helper function to try to connect to a server from confirguation file
int try_connect(char *ipaddr, char *port, int idx)
{
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    sockfd[idx] = socket(AF_INET, SOCK_STREAM, 0);
    // printf("%d\n", sockfd[idx]);
    bzero(&servers[idx], sizeof(servers[idx]));

    if (setsockopt(sockfd[idx], SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        // printf("ERROR SETTING SOCKET TIMEOUT\n");
    }

    struct hostent *host = gethostbyname(ipaddr);
    if (host == NULL)
        error("Error resolving hostname\n");
    servers[idx].sin_family = AF_INET;
    bcopy((char *)host->h_addr, &servers[idx].sin_addr.s_addr, host->h_length);
    servers[idx].sin_port = htons(atoi(port));

    if (connect(sockfd[idx], (struct sockaddr *)&servers[idx], sizeof(struct sockaddr_in)) != 0)
    {
        // printf("Error in connect to server %d\n", idx + 1);
        return 0;
    }
    status[idx] = 1;
    return 1;
}

// read configuration file and try to connect to servers in file, update number of live servers based off return value of try_connect
void read_conf()
{
    FILE *fp = fopen(strcat(getenv("HOME"), "/dfc.conf"), "rb");
    if (fp == NULL)
    {
        return;
    }
    size_t len;
    ssize_t read;
    char *line = NULL;
    // if configuration file doesn't exist, we can't do anything.
    if (fp == NULL)
    {
        error("ERROR: Cannot find dfc.conf file in current working directory.\n");
    }
    int lineno = 0;
    while ((read = getline(&line, &len, fp)) > 0)
    {
        char *ip_ptr = strrchr(line, ' ');
        if (ip_ptr == NULL)
        {
            lineno++;
            continue;
        }
        ip_ptr++;

        char *port_ptr = strrchr(line, ':');
        if (port_ptr == NULL)
        {
            lineno++;
            continue;
        }
        port_ptr++;
        size_t l = strlen(ip_ptr) - strlen(port_ptr) - 1;
        if (l <= 0)
        {
            lineno++;
            continue;
        }
        char *temp_ip = malloc(l);
        memcpy(temp_ip, ip_ptr, l);

        if (strlen(port_ptr) && (port_ptr[strlen(port_ptr) - 1] == '\n'))
            port_ptr[strlen(port_ptr) - 1] = 0;

        connections += try_connect(temp_ip, port_ptr, lineno);
        lineno++;
        free(temp_ip);
    }

    if (line)
        free(line);

    fclose(fp);
    return;
}

/* hash function from piazza - http://www.cse.yorku.ca/~oz/hash.html */
unsigned long
hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

// helper function to close all connections to servers before exiting client program
void _close()
{
    char ackbuf[16];
    bzero(&ackbuf, 16);

    for (int k = 0; k < 4; k++)
    {
        if (status[k])
        { // send -FIN to server to break recursive loop & free connection
            write(sockfd[k], "-FIN", 4);
            read(sockfd[k], ackbuf, 7);
            if (strcmp(ackbuf, "FIN ACK"))
            {
                // printf("Received FIN ACK from server %d\n", k + 1);
                close(sockfd[k]);
            }
        }
    }
}

void handle_list()
{
    // printf("Inside handle_list\n");
    char buf[256];
    char ackbuf[16];
    struct fileCount files[512];
    ssize_t numRead;
    bzero(&buf, 256);
    bzero(&ackbuf, 16);
    // Idea: get all files from servers and store in memory, output list of files after receiving all from all active servers
    for (int k = 0; k < 4; k++)
    {
        if (status[k])
        { // tell server we are trying to do a list
            write(sockfd[k], "LIST", 4);
            numRead = read(sockfd[k], ackbuf, 8);
            if (numRead < 0)
            {
                status[k] = 0;
                connections--;
                continue;
            }
            bzero(&ackbuf, 8);

            write(sockfd[k], "+ACK", 4);
            numRead = read(sockfd[k], ackbuf, 4);
            if (numRead < 0)
            {
                status[k] = 0;
                connections--;
                continue;
            }
            bzero(&ackbuf, 4);

            while (1)
            {
                numRead = read(sockfd[k], buf, 256);
                if (numRead < 0)
                {
                    status[k] = 0;
                    connections--;
                    continue;
                }
                if (strcmp(buf, "FIN") == 0)
                    break;
                // logic to extract chunk number from filename (-1 for arithmetic in loop)
                char *cPtr = strrchr(buf, '-');
                cPtr++;
                int chunkno = atoi(cPtr) - 1;

                // copy filename without chunk number into temp variable
                char temp[254];
                bzero(&temp, 254);
                strncpy(temp, buf, strlen(buf) - 2);
                // loop over in memory structure of files
                for (int i = 0; i < 512; i++)
                { // file already exists
                    if (strcmp(files[i].fname, temp) == 0)
                    { // make sure we have all pieces that we need
                      // idea: 15 = 1111 in binary, each bit represents whether we have a chunk from the server or not
                      // just |= our check with 1 << chunkNumber to set that bit if it hasn't been set already
                        if (files[i].check != 15)
                            files[i].check |= (1 << chunkno);
                        break;
                    }
                    else if (strlen(files[i].fname) == 0) // did not find file and found an empty place in memory, just add the file not seen to this location
                    {                                     // initialize file in structure of files
                        strcpy(files[i].fname, temp);
                        files[i].check = 0;
                        files[i].check |= (1 << chunkno);
                        break;
                    }
                }
                write(sockfd[k], "+ACK", 4);
                bzero(&buf, strlen(buf));
            }
        }
    }
    // after receiving all files from all servers, print out files received & incomplete files
    for (int k = 0; k < 512; k++)
    {
        if (strlen(files[k].fname) == 0)
            break;
        else if (files[k].check == 15) // 1111 -> all chunks seen
            printf("%s\n", files[k].fname);
        else // != 15 -> missing at least one chunk somewhere
            printf("%s [incomplete]\n", files[k].fname);
    }
    _close();
}

void handle_get(char **files, int numFiles, int flag)
{
    char buf[BUFSIZE];
    char fname[BUFSIZE];
    char tempFname[CSIZE];
    char timestamp[32];
    char ackbuf[16];
    ssize_t numRead;
    int delFlag = flag;
    for (int i = 0; i < numFiles; i++)
    {
        if (delFlag || (connections < 3))
        {
            printf("%s is incomplete\n", files[i]);
            remove(files[i]);
            continue;
        }
        FILE *fp = fopen(files[i], "wb"); // open file to write to
        // loop over chunk numbers 1-lines (lines = 4)
        for (int chunknum = 1; chunknum <= lines; chunknum++)
        {
            if (chunknum == 1)
            {
                sprintf(fname, "%s-%d", files[i], chunknum); // request the first chunk of the file
            }
            else // after we receive the first chunk, we also have the timestamp associated with this chunk. Add that to future requests to make sure
            // all parts are from the same file with the same time stamp to ensure no loss of data or jumbling of multiple different files with different timestamps but same name.
            {
                sprintf(fname, "%s-%s-%d", timestamp, files[i], chunknum);
            }
            int seenFlag = 0;
            // loop over servers
            for (int k = 0; k < 4; k++)
            {
                if (status[k]) // if server is active, try to get a chunk.
                {
                    bzero(&ackbuf, 16);
                    send(sockfd[k], "GET", 3, 0);
                    numRead = recv(sockfd[k], ackbuf, 7, 0);
                    if (numRead < 0) // checking if server goes down during transfer
                    {
                        status[k] = 0;
                        connections--;
                        continue;
                    }
                    bzero(&ackbuf, 16);
                    send(sockfd[k], fname, strlen(fname), 0);       // send filename + chucnk to server
                    numRead = recv(sockfd[k], tempFname, CSIZE, 0); // determine of server has it
                    if (numRead < 0)
                    {
                        status[k] = 0;
                        connections--;
                        continue;
                    }
                    if (strcmp(tempFname, "-ACK") == 0) // server does not have it, try next server
                        continue;
                    else // this server does have the chunk we want
                    {
                        if (chunknum == 1) // extract timestamp from tempFname
                        {
                            char *ptr = tempFname;
                            char *temp = strsep(&ptr, "-");
                            if (temp != NULL)
                                strncpy(timestamp, temp, strlen(temp));
                        }
                    }
                    bzero(&tempFname, CSIZE);
                    numRead = read(sockfd[k], buf, BUFSIZE);
                    if (numRead < 0)
                    {
                        status[k] = 0;
                        connections--;
                        continue;
                    }
                    // get number of bytes in chunk for client
                    size_t numBytes = (size_t)atoi(buf);
                    bzero(&buf, BUFSIZE);
                    size_t bytesRead;
                    send(sockfd[k], "+ACK", 4, 0);
                    // simply write to the file we opened until we encounter an acknowledgement from the server
                    while ((bytesRead = read(sockfd[k], buf, BUFSIZE)) > 0)
                    {
                        if (strcmp(buf, "FIN") == 0)
                        {
                            send(sockfd[k], "FIN", 3, 0);
                            seenFlag = 1;
                            break;
                        }
                        numBytes -= fwrite(buf, sizeof(char), bytesRead, fp);
                        send(sockfd[k], "+ACK", 4, 0);
                        bzero(&buf, bytesRead);
                    }
                    if (bytesRead < 0)
                    {
                        status[k] = 0;
                        connections--;
                        continue;
                    }
                    // break out of this loop as we got the chunk we needed, go onto the next chunk
                    break;
                }
            }
            // no servers had the chunk we asked for - can't get file. set delete flag and notify client that this failed.
            if (!seenFlag)
            {
                delFlag = 1;
                break;
            }
            bzero(&fname, strlen(fname));
        }

        fclose(fp);
    }
    _close();
}

void handle_put(char **files, int numFiles, int flag)
{
    int errFlag = flag;
    char buf[BUFSIZE];
    char headers[CSIZE];
    bzero(&buf, BUFSIZE);
    bzero(&headers, CSIZE);
    unsigned long filehash;
    int mod;
    struct timeval curTime;
    // loop over all files supplied in command line
    for (int i = 0; i < numFiles; i++)
    {
        if (errFlag)
        {
            printf("%s put failed.\n", files[i]);
            continue;
        }
        // for timestamp
        gettimeofday(&curTime, NULL);
        FILE *fp = fopen(files[i], "rb");
        if (fp == NULL)
        {
            printf("Error: file '%s' does not exist\n", files[i]);
        }
        else
        {
            filehash = hash(files[i]);
            mod = filehash % connections;
            int startidx = mod;
            int other_server = mod - 1;
            if (mod == 0)
                other_server = connections - 1;

            int p1 = 1;
            int p2 = 2;
            // determine where to send chunks to based off of number of connections
            // little function I wrote to dynamicaly determine where to send chunks to
            // note: since we are always dealing with 4 servers, this isn't necessary, but I wrote this before I knew that we would always have 4 servers.
            // this will work even if there is only 3 connections, but my get would not work because I don't have metadata files on my servers.
            increment(&p1, 0);
            increment(&p2, 0);
            for (int k = 0; k < connections; k++)
            {
                char chunkpair[4];
                sprintf(chunkpair, "%d,%d", p1, p2);
                addPair(chunkpair, startidx, &chunkList[startidx]);

                startidx = (startidx + 1) % connections;
                (other_server == 0) ? other_server = connections - 1 : other_server--;
                increment(&p1, 1);
                increment(&p2, 1);
            }

            // logic to send file here
            // calculate bytes & determine chunk sizes
            fseek(fp, 0, SEEK_END);
            long numBytes = ftell(fp);
            long chunkSize = numBytes / lines;
            long remainder = numBytes % lines;
            // if not evenly divisible by 4, increase chunksize for first remainder chunks (each will have 1 extra byte)
            if (remainder != 0)
                chunkSize++;

            fseek(fp, 0, SEEK_SET);
            for (int k = 0; k < lines; k++)
            {
                char s1ack[16];
                char s2ack[16];
                size_t sent1, sent2, rec1, rec2, bytesRead;
                if (remainder == 0 && k != 0) // if we don't have any extra bytes to send, just decrease the chunk size by 1
                    chunkSize--;
                int s1 = atoi(&chunkList[k].chunks[0]);
                int s2 = atoi(&chunkList[k].chunks[2]);

                // printf("Trying to send chunk %d to %d %d\n", k + 1, s1, s2);
                /* Notify servers that they are receiving a put */
                sent1 = send(sockfd[s1 - 1], "PUT", 3, 0);
                sent2 = send(sockfd[s2 - 1], "PUT", 3, 0);

                /* Receive ack from servers that they are ready to handle put */
                rec1 = recv(sockfd[s1 - 1], s1ack, CSIZE, 0);
                if (rec1 < 0)
                {
                    fclose(fp);
                    status[s1 - 1] = 0;
                    errFlag = 1;
                    connections--;
                    printf("%s put failed.\n", files[i]);
                    break;
                }

                rec2 = recv(sockfd[s2 - 1], s2ack, CSIZE, 0);
                if (rec2 < 0)
                {
                    fclose(fp);
                    status[s2 - 1] = 0;
                    errFlag = 1;
                    connections--;
                    printf("%s put failed.\n", files[i]);
                    break;
                }
                bzero(&s1ack, strlen(s1ack));
                bzero(&s2ack, strlen(s2ack));

                // send headers to servers so they can create file for writing on server side
                long remaining = chunkSize;
                sprintf(headers, "%s %d %ld_%ld %ld", files[i], (k + 1), curTime.tv_sec, curTime.tv_usec, chunkSize);
                sent1 = send(sockfd[s1 - 1], headers, strlen(headers), 0);
                sent2 = send(sockfd[s2 - 1], headers, strlen(headers), 0);
                // printf("Send headers\n");
                rec1 = recv(sockfd[s1 - 1], s1ack, CSIZE, 0);
                if (rec1 < 0)
                {
                    fclose(fp);
                    status[s1 - 1] = 0;
                    errFlag = 1;
                    connections--;
                    printf("%s put failed.\n", files[i]);
                    break;
                }
                rec2 = recv(sockfd[s2 - 1], s2ack, CSIZE, 0);
                if (rec2 < 0)
                {
                    fclose(fp);
                    status[s2 - 1] = 0;
                    errFlag = 1;
                    connections--;
                    printf("%s put failed.\n", files[i]);
                    break;
                }
                bzero(&s1ack, strlen(s1ack));
                bzero(&s2ack, strlen(s2ack));

                bzero(&headers, strlen(headers));

                /* Recieved ack from both servers */
                bzero(&buf, BUFSIZE);
                // send file to both servers at the same time. Checks if a server goes down mid-put and handles error accordingly.
                while (remaining > BUFSIZE)
                {
                    bytesRead = fread(buf, 1, BUFSIZE, fp);
                    remaining -= bytesRead;
                    // printf("Bytes read: %ld\n", bytesRead);
                    sent1 = send(sockfd[s1 - 1], buf, bytesRead, 0);
                    sent2 = send(sockfd[s2 - 1], buf, bytesRead, 0);
                    // receive ack before sending another packet
                    rec1 = recv(sockfd[s1 - 1], s1ack, 4, 0);
                    if (rec1 < 0)
                    {
                        fclose(fp);
                        status[s1 - 1] = 0;
                        errFlag = 1;
                        connections--;
                        printf("%s put failed.\n", files[i]);
                        break;
                    }
                    rec2 = recv(sockfd[s2 - 1], s2ack, 4, 0);
                    if (rec2 < 0)
                    {
                        fclose(fp);
                        status[s2 - 1] = 0;
                        errFlag = 1;
                        connections--;
                        printf("%s put failed.\n", files[i]);
                        break;
                    }
                    bzero(&s1ack, 4);
                    bzero(&s2ack, 4);
                    bzero(&buf, BUFSIZE);
                }
                // continue to send remainder of file (<BUFSIZE)
                while (remaining > 0)
                {
                    bytesRead = fread(buf, 1, remaining, fp);
                    remaining -= bytesRead;
                    sent1 = send(sockfd[s1 - 1], buf, bytesRead, 0);
                    sent2 = send(sockfd[s2 - 1], buf, bytesRead, 0);
                    // receive ack before sending more (if needed)
                    rec1 = recv(sockfd[s1 - 1], s1ack, 4, 0);
                    if (rec1 < 0)
                    {
                        fclose(fp);
                        status[s1 - 1] = 0;
                        errFlag = 1;
                        connections--;
                        printf("%s put failed.\n", files[i]);
                        break;
                    }
                    rec2 = recv(sockfd[s2 - 1], s2ack, 4, 0);
                    if (rec2 < 0)
                    {
                        fclose(fp);
                        status[s2 - 1] = 0;
                        errFlag = 1;
                        connections--;
                        printf("%s put failed.\n", files[i]);
                        break;
                    }
                    bzero(&s1ack, 4);
                    bzero(&s2ack, 4);
                    bzero(&buf, BUFSIZE);
                }
                // just sent a chunk with an extra byte due to remainder - decrement count of bytes to send
                if (remainder >= 0)
                    remainder--;
                // done sending chunk - notify server that we are finished
                send(sockfd[s1 - 1], "PUT FIN ACK", 11, 0);
                send(sockfd[s2 - 1], "PUT FIN ACK", 11, 0);
                // receive acknowledgement from server that we are done
                rec1 = recv(sockfd[s1 - 1], s1ack, 11, 0);
                if (rec1 < 0)
                {
                    fclose(fp);
                    status[s1 - 1] = 0;
                    errFlag = 1;
                    connections--;
                    printf("%s put failed.\n", files[i]);
                    break;
                }
                rec2 = recv(sockfd[s2 - 1], s2ack, 11, 0);
                if (rec2 < 0)
                {
                    fclose(fp);
                    status[s2 - 1] = 0;
                    errFlag = 1;
                    connections--;
                    printf("%s put failed.\n", files[i]);
                    break;
                }
                // printf("Sent chunk %d to server %d %d\n", k + 1, s1, s2);
            }
            if (!errFlag)
                fclose(fp);
        }
    }
    _close();
    return;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        error("Error: incorrect usage.\nProper usage: ./dfc <command> [filename] ... [filename]\n\n");
    }
    int numFiles = argc - 2;

    read_conf();

    if (!strcmp(argv[1], "list"))
    {
        (argc == 2) ? handle_list() : error("Error: cannot supply filenames after 'list' command.\n");
    }
    else if (!strcmp(argv[1], "get"))
    {
        (connections >= (lines - 2)) ? handle_get(&argv[2], numFiles, 0) : handle_get(&argv[2], numFiles, 1);
    }
    else if (!strcmp(argv[1], "put"))
    {
        (connections == lines) ? handle_put(&argv[2], numFiles, 0) : handle_put(&argv[2], numFiles, 1);
    }
    else
    {
        error("Error: incorrect usage.\nProper usage: ./dfc <command> [filename] ... [filename]\nProper commands: 'list', 'get', 'put'\n");
    }

    return 0;
}