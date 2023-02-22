# Network Systems Fall 2022 Programming Assignments | Hampton Walker

All code is my own original work unless specifically stated otherwise with comments in my code (I ripped a hashing function from GeeksForGeeks and some code from StackOverflow for recursively removing a directory, which was outside of the bounds for the assignments but something that I wanted to include). All assignments were done in C. Note that there was starter files for the UDP File Transfer assignment (really just importing libraries and the code for binding sockets), but there was no other code given for any other assignments.

I achieved the highest score of my classmates on all assignments except the TCP Webserver (I only `read()` once instead of looping until I found a CLRF in the request - an oversight on my end that was fixed in the Caching Proxy). The average grade for most assignments was in the 60's.

## Requirements for Running Code

You need to be on a Mac on Linux OS, as Windows does not support many of the libraries I used for these assignments (`sys/socket.h`, `arpa/inet.h` are two examples). Since I natively run Windows, I did all my work on an Ubuntu VM which worked perfectly for these assignments.

## Links to assignments

- [PA1 - UDP Client/Server File Transfer | 100/100](./UDP_File_Transfer)

- [PA2 - TCP Web Server | 85/100](./TCP_Webserver)

- [PA3 - TCP Caching Web Proxy | 98/100](./TCP_Caching_Proxy)

- [PA4 - TCP Distributed File Server | 102/100](./TCP_Distributed_File_Server)

### Disclaimer

I do not authorize any current CSCI 4273 student to plagarize my code. This repository is solely to show what I accomplished in what some students call "the most difficult Core Computer Science class offered by the University".
