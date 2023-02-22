/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BUFSIZE 8192

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/* Functionality for sendto, just used a lot so made it's own function for little duplication*/
void send_data(int fd, char* buf, struct sockaddr_in clientaddr, int clientlen, int flag) {
  // printf("Trying to send: %s\n", buf);
  if (!flag) {
    int n = sendto(fd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
    printf("Bytes sent: %d\n", n);
  } else {
    int n = sendto(fd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
    printf("Bytes sent: %d\n", n);
  }
}

/* ls command handling from server side */
/* Implementation inspired by the following source:
 * https://pubs.opengroup.org/onlinepubs/009696799/functions/popen.html */
void list_files(int fd, char* buf, struct sockaddr_in clientaddr, int clientlen) {
    send_data(fd, buf, clientaddr, clientlen, 1);
    FILE *fp;
    char *temp = (char *)malloc(BUFSIZE);
    bzero(temp, BUFSIZE);

    fp = popen("ls -la", "r");
    if (fp == NULL){
      error("Error running command: 'ls -la'\n");
    }

    bzero(buf, BUFSIZE);
    while (fgets(temp, BUFSIZE, fp) != NULL) {
      strncpy(buf, temp, strlen(temp) - 1); // remove trailing newline character, copy into buf
      send_data(fd, buf, clientaddr, clientlen, 1); // send data back into 
      bzero(buf, BUFSIZE);
      bzero(temp, BUFSIZE);
    }

    free(temp);
    if (pclose(fp) == -1) {
      error("Error closing popen command\n");
    }

    bzero(buf, BUFSIZE);
    strcpy(buf, "...DONE LISTING FILES...");
    send_data(fd, buf, clientaddr, clientlen, 1);
}

void delete_file(char *buf, int fd, struct sockaddr_in clientaddr, int clientlen) {
  send_data(fd, buf, clientaddr, clientlen, 1);

  char *bufcopy = (char *)malloc(BUFSIZE);
  bzero(bufcopy, BUFSIZE);
  strncpy(bufcopy, buf, BUFSIZE);
  // printf("Echo from user: %s\n", buf);
  char *searcher = strtok(bufcopy, " ");
  searcher = strtok(NULL, " ");

  char *fn = (char *)malloc(BUFSIZE);
  bzero(fn, BUFSIZE);
  strncpy(fn, searcher, strlen(searcher) -1);

  searcher = strtok(NULL, " ");
  if (searcher == NULL) {
    if(remove(fn) == 0) {
      bzero(buf, BUFSIZE);
      strcpy(buf, "DELETED FILE");
      send_data(fd, buf, clientaddr, clientlen, 1);
    } else {
      bzero(buf, BUFSIZE);
      strcpy(buf, "FILE NOT FOUND");
      send_data(fd, buf, clientaddr, clientlen, 1);
    }
  } else {
    bzero(buf, BUFSIZE);
    strcpy(buf, "ERROR IN COMMAND");
    send_data(fd, buf, clientaddr, clientlen, 1);
  }
  free(bufcopy);
  free(fn);
}

void get_file(char *buf, int fd, struct sockaddr_in clientaddr, int clientlen) {
  send_data(fd, buf, clientaddr, clientlen, 1);
  char *bufcopy = (char *)malloc(BUFSIZE);
  bzero(bufcopy, BUFSIZE);
  strncpy(bufcopy, buf, BUFSIZE);

  char *searcher = strtok(bufcopy, " ");
  searcher = strtok(NULL, " ");

  char *path = (char *)malloc(BUFSIZE);
  bzero(path, BUFSIZE);
  strncpy(path, searcher, strlen(searcher) -1);

  searcher = strtok(NULL, " ");
  if (searcher == NULL) { // command useage correctly, check if file exists
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
      // File not found, send info to user and exit
      bzero(buf, BUFSIZE);
      strcpy(buf, "ERROR OPENING FILE");
      printf("%s\n", buf);
      send_data(fd, buf, clientaddr, clientlen, 1);
      return;
    }
    // file exists
    // read amount of bytes in file
    fseek(fp, 0, SEEK_END);
    long int numBytes = ftell(fp);
    //send number of bytes to user
    bzero(buf, BUFSIZE);
    sprintf(buf, "%ld", numBytes);
    send_data(fd, buf, clientaddr, clientlen, 1);
    
    // reset file position to start of file
    fseek(fp, 0, SEEK_SET);
    printf("%ld\n", ftell(fp));
    
    /* extract filename */
    char *pathcopy = (char *)malloc(strlen(path));
    strcpy(pathcopy, path);
    char *temp = strtok(pathcopy, "/");

    if(!!strcmp(path, pathcopy)) {
      // printf("Need to find filename.\n");
      char *fn = (char *)malloc(1024);
      while((temp = strtok(NULL, "/")) != NULL) {
        bzero(fn, sizeof(fn));
        strcpy(fn, temp);
      }
      bzero(buf, BUFSIZE);
      strcpy(buf, fn);
      send_data(fd, buf, clientaddr, clientlen, 1);
      free(fn);
    } else {
      // printf("Don't need to send filename.\n");
      bzero(buf, BUFSIZE);
      strcpy(buf, path);
      send_data(fd, buf, clientaddr, clientlen, 1);
    }
    free(pathcopy);

    /* Wait for client to acknowledge opening the file on their end */
    bzero(buf, BUFSIZE);
    int n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* Exit if error on user end */
    if(!strcmp(buf, "ERROR OPENING FILE")) { 
      printf("Echo from client: %s\n", buf);
      fclose(fp);
      return;
    }
    while(numBytes != ftell(fp)) {
      // printf("In loop\n");
      bzero(buf, BUFSIZE);
      fread(buf, sizeof(char), BUFSIZE, fp);
      // printf("%s\n", buf);
      send_data(fd, buf, clientaddr, clientlen, 0);
      
      bzero(buf, BUFSIZE);
      // get response from user
      n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
      if (n < 0)
        error("ERROR in recvfrom");
      printf("Bytes received: %d %s\n", n, buf);
      printf("Current file position: %ld\n", ftell(fp));
    }
    // printf("Out of loop\n");
    strcpy(buf, "...DONE SENDING FILE...RETURN CONTROL...");
    send_data(fd, buf, clientaddr, clientlen, 1);
    fclose(fp);
  } else { // error in command
    bzero(buf, BUFSIZE);
    strcpy(buf, "ERROR IN COMMAND");
    send_data(fd, buf, clientaddr, clientlen, 1);
  }
  free(bufcopy);
  free(path);
}

void put_file(char *buf, int fd, struct sockaddr_in clientaddr, int clientlen) {
  send_data(fd, buf, clientaddr, clientlen, 1);
  int n;
  bzero(buf, BUFSIZE);

  n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
  if (n < 0) {
    error("ERROR in recvfrom");
  }
  printf("Echo from server: %s\n", buf);
  if(!strcmp(buf, "ERROR OPENING FILE")) {
    return;
  } 
  if (!strcmp(buf, "ERROR IN COMMAND")) {
    return;
  }
  // this holds number of bytes if not an error
  // add code to check for error messages
  int numBytes = atoi(buf);
  printf("Number of bytes in file: %d\n", numBytes);
  bzero(buf, BUFSIZE);

  // now buf holds filename
  n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
  if (n < 0) {
    error("ERROR in recvfrom");
  }
  // printf("Echo from server: %s\n", buf);
  
  char *fn = (char *)malloc(strlen(buf));
  strcpy(fn, buf);
  printf("Filename: %s\n", fn);

  FILE *fp = fopen(fn, "wb");
  if(fp == NULL) {
    bzero(buf, BUFSIZE);
    strcpy(buf, "ERROR OPENING FILE");
    send_data(fd, buf, clientaddr, clientlen, 1);
    free(fn);
    return;
  }
  bzero(buf, BUFSIZE);
  strcpy(buf, "READY FOR FILE");
  send_data(fd, buf, clientaddr, clientlen, 1);
  // now get ready to read data from buffer into file
  // turn this into a loop based off of numBytes / BUFSIZE
  printf("Waiting for buffer\n");
  while(1) {
    bzero(buf, BUFSIZE);
    n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
    if (n < 0) {
      error("ERROR in recvfrom");
    }
    // printf("Echo from server: %s\n", buf);
    if(!strcmp(buf, "...DONE SENDING FILE...RETURN CONTROL...")) {
      printf("Exiting get.\n");
      free(fn);
      fclose(fp);
      return;
    }
    if (numBytes < BUFSIZE) {
      numBytes -= (int) fwrite(buf, sizeof(char), numBytes, fp);
      printf("Num bytes: %d\n", numBytes);
    } else {
      numBytes -= (int) fwrite(buf, sizeof(char), n, fp);
      printf("Num bytes: %d\n", numBytes);
    }
    printf("Bytes received: %d\n", n);
    printf("Buffer data: %s\n", buf);
    printf("Current file position: %ld\n", ftell(fp));

    bzero(buf, BUFSIZE);
    strcpy(buf, "READY");
    send_data(fd, buf, clientaddr, clientlen, 1);
  }
}

void handle_command(char *buf, int fd, struct sockaddr_in clientaddr, int clientlen) {
  int n = 0; 
  char *bufcopy = (char* )malloc(BUFSIZE);
  bzero(bufcopy, BUFSIZE);
  strncpy(bufcopy, buf, strlen(buf));
  char *searcher = strtok(bufcopy, " ");
  /* EXIT COMMAND HANDLING */
  if (!strcmp(buf, "exit\n")) {
    /* Echo exit back to client, client will handle exiting by closing socket file descriptor. Server stays open. */
    send_data(fd, buf, clientaddr, clientlen, 1);
  } 
  /* LS COMMAND HANDLING */
  else if (!strcmp(buf, "ls\n")) {
    list_files(fd, buf, clientaddr, clientlen);
  }
  /* GET COMMAND HANDLING */
   else if (!strcmp(searcher, "get")) {
    printf("Got a get command\n");
    get_file(buf, fd, clientaddr, clientlen);
  }
  /* PUT COMMAND HANDLING */
  else if (!strcmp(searcher, "put")) {
    printf("Got a put command\n");
    put_file(buf, fd, clientaddr, clientlen);
  }
  /* DELETE COMMAND HANDLING */
  else if (!strcmp(searcher, "delete")) {
    printf("Got a delete command\n");
    delete_file(buf, fd, clientaddr, clientlen);
  } else {
    bzero(buf, 1024);
    strcpy(buf, "unknown command received\n");
    // printf("unknown command received\n");
    n = sendto(fd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) {
      free(bufcopy);
      error("ERROR in sendto");
    }
  }
  free(bufcopy);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %ld/%d bytes: %s\n", strlen(buf), n, buf);
    
    /* Debugging: print message received from user */
    printf("Buffer contents: %s\n", buf);
    handle_command(buf, sockfd, clientaddr, clientlen);
  }
}
