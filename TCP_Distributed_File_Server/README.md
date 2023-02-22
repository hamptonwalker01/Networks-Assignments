# CSCI 4273 | PA4 - Distributed File System| Hampton Walker

This is my solution to PA4 for CSCI 4273 at CU Boulder. As stated in the writeup, the solution is coded around a static amount of 4 servers. Some of my code is dynamic, as I did not think we could assume only 4 servers at once.

For this assignment, a client wants to store files on a distributed file server. Files are split into 4 roughly equal chunks (an extra byte on some chunks if filesize is not divisible by 4), and each chunk is sent to 2 different servers depending on the hash (e.g. for one file, chunk 1 goes to servers 1 and 2, where another file's hash sends chunk 1 to servers 3 and 4). Refer to the following table to see where the chunks go:

| Filename % 4 |            0 | 1            | 2            | 3            |
| :----------- | -----------: | ------------ | ------------ | ------------ |
| **DFS1**     | Chunks (1,2) | Chunks (4,1) | Chunks (3,4) | Chunks (2,3) |
| **DFS2**     | Chunks (2,3) | Chunks (1,2) | Chunks (4,1) | Chunks (3,4) |
| **DFS3**     | Chunks (3,4) | Chunks (2,3) | Chunks (1,2) | Chunks (4,1) |
| **DFS4**     | Chunks (4,1) | Chunks (3,4) | Chunks (2,3) | Chunks (1,2) |

## Compiling & Usage

Compile `client/dfc.c` and `server/dfs.s` with gcc as usual. As for the `dfc.conf` file, move this file to your `$HOME` path.

Start up to 4 dfs servers. DFS servers are started with `./dfs [dirname] [port]`, where `[dirname]` looks like `dfsN`, where N is the server number (1-4). Be sure to specify the same port and have the servers running on the same address as specified in `dfc.conf`. **This is necessary for the client to function properly.**

Once your servers are up, the client is run the following way: `./dfc [command] [...files]` and supports the following commands:

- `./dfc get [...files]`: requires at least 3 servers up to reliably get file(s) from the servers. Supply an arbitrary number of filenames after get and the client tries to get all of those files. Client will print an error if it was not able to retrieve a file.

- `./dfc put [...files]`: requires all 4 servers up to reliably put files onto the server. Supply an arbitrary number of filenames after put and the client tries to put all of those files. Client will print an error if it was not able to put a file (not enough servers or server(s) go down mid put).

- `./dfc ls`: lists files from servers. If a file is not able to be retrieved reliably (not enough servers up), the client will print that the file is incomplete. If no servers are up, nothing happens. With at least 2 servers up, you should see all files you have put onto the servers, though some may be incomplete (a server is down that holds a chunk).

## Test Script

To test getting and putting onto servers, simply put the executable and `pa4test` in a directory with other files. `pa4test` will put the files onto the servers, make backups of files, and then get and diff files from servers. Expected output is nothing. If you see output, then there is an error. Be sure to run the test script with all 4 servers up or else there will be issues.

## Grade: 102/100

## Class Average: 49.5/100

While there was no extra credit, I got two extra points somehow. Nice!
