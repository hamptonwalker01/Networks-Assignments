/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define BUFSIZE 8192

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

/* Functionality for sendto, just used a lot so made it's own function for little duplication*/
void send_data(int fd, char* buf, struct sockaddr_in serveraddr, int serverlen, int flag) {
  // printf("Trying to send: %s\n", buf);
  if (!flag) {
    int n = sendto(fd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    printf("Bytes sent: %d\n", n);
  } else {
    int n = sendto(fd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    printf("Bytes sent: %d\n", n);
  }
}

/* Print User Commands to Terminal, send command to server */
void prompt_user(char *buf, int fd, struct sockaddr_in serveraddr) {
  bzero(buf, BUFSIZE);
  printf("\nPlease enter one of the following commands:\n");
  printf("get [file_name]\n");
  printf("put [file_name]\n");
  printf("delete [file_name]\n");
  printf("ls\n");
  printf("exit\n\n");
  fgets(buf, BUFSIZE, stdin);

  /* send the message to the server */
  int serverlen = sizeof(serveraddr);
  send_data(fd, buf, serveraddr, serverlen, 1);
}

/* Client handling of ls command */
void listen_for_files(char *buf, int fd, struct sockaddr_in serveraddr) {
  int serverlen = sizeof(serveraddr);
  int n;
  printf("\n");
  while(strcmp(buf, "...DONE LISTING FILES...") != 0) {
    bzero(buf, BUFSIZE);
    n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
    if (n < 0) {
      error("ERROR in recvfrom");
    }
    if(strcmp(buf, "...DONE LISTING FILES...") != 0) {
      printf("%s\n", buf);
    }
  }  
  printf("Echo from server: %s\n", buf);  
}

void handle_delete(char *buf, int fd, struct sockaddr_in serveraddr)  {
  int serverlen = sizeof(serveraddr);
  int n;
  // printf("In handle delete...\n");
  bzero(buf, BUFSIZE);
  n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
  if (n < 0) {
    error("ERROR in recvfrom");
  }
  printf("Echo from server: %s\n", buf);
}

void handle_put(char *buf, int fd, struct sockaddr_in serveraddr) {
  int serverlen = sizeof(serveraddr);
  int n;
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
      send_data(fd, buf, serveraddr, serverlen, 1);
      return;
    }
    // file exists
    // read amount of bytes in file
    fseek(fp, 0, SEEK_END);
    long int numBytes = ftell(fp);
    //send number of bytes to user
    bzero(buf, BUFSIZE);
    sprintf(buf, "%ld", numBytes);
    send_data(fd, buf, serveraddr, serverlen, 1);
    
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
      send_data(fd, buf, serveraddr, serverlen, 1);
      free(fn);
    } else {
      // printf("Don't need to send filename.\n");
      bzero(buf, BUFSIZE);
      strcpy(buf, path);
      send_data(fd, buf, serveraddr, serverlen, 1);
    }
    free(pathcopy);
    /* Wait for client to acknowledge opening the file on their end */
    bzero(buf, BUFSIZE);
    int n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
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
      send_data(fd, buf, serveraddr, serverlen, 0);
      
      bzero(buf, BUFSIZE);
      // get response from user
      n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
      if (n < 0)
        error("ERROR in recvfrom");
      printf("Bytes received: %d %s\n", n, buf);
      printf("Current file position: %ld\n", ftell(fp));
    }
    // printf("Out of loop\n");
    strcpy(buf, "...DONE SENDING FILE...RETURN CONTROL...");
    send_data(fd, buf, serveraddr, serverlen, 1);
    fclose(fp);

  } else { // error in command
    bzero(buf, BUFSIZE);
    strcpy(buf, "ERROR IN COMMAND");
    send_data(fd, buf, serveraddr, serverlen, 1);
  }
  free(bufcopy);
  free(path);
}

void handle_get(char *buf, int fd, struct sockaddr_in serveraddr) {
  int serverlen = sizeof(serveraddr);
  int n;
  bzero(buf, BUFSIZE);

  n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
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
  n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
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
    send_data(fd, buf, serveraddr, serverlen, 1);
    free(fn);
    return;
  }
  bzero(buf, BUFSIZE);
  strcpy(buf, "READY FOR FILE");
  send_data(fd, buf, serveraddr, serverlen, 1);
  // now get ready to read data from buffer into file
  // turn this into a loop based off of numBytes / BUFSIZE
  printf("Waiting for buffer\n");
  while(1) {
    bzero(buf, BUFSIZE);
    n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
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
    send_data(fd, buf, serveraddr, serverlen, 1);
  }
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    
    while(1) {
      /* get a message from the user */
      prompt_user(buf, sockfd, serveraddr);
      bzero(buf, BUFSIZE);
      /* print the server's reply */
      n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
      if (n < 0) 
        error("ERROR in recvfrom");
      printf("Echo from server: %s\n", buf);
      char *bufcopy = (char * )malloc(BUFSIZE);

      bzero(bufcopy, BUFSIZE);
      strcpy(bufcopy, buf);
      char *searcher = strtok(bufcopy, " ");
      // printf("Searcher: %s\n", searcher);

      /* Logic for function to call based off of response from server */
      // will move this to its own function after more commands have been implemented
      if (!strcmp(searcher, "delete")) {
        printf("Trying to call handle delete...\n");
        handle_delete(buf, sockfd, serveraddr);
      } 
      if (!strcmp(searcher, "get")) {
        printf("Trying to call get file...\n");
        handle_get(buf, sockfd, serveraddr);
      }
      if (!strcmp(searcher, "put")) {
        printf("Trying to call put file...\n");
        handle_put(buf, sockfd, serveraddr);
      }
      /* ls handling */
      int result = strcmp(buf, "ls\n");
      if (result == 0) {
        listen_for_files(buf, sockfd, serveraddr);
      }
      /* Exit handling */
      result = strcmp(buf, "exit\n");
      if(result == 0) {
        if(close(sockfd) == 0) {
          printf("Exiting... Goodbye!\n");
          return 0;
        } else error("Error closing socket");
      }
      free(bufcopy);
    }
}
