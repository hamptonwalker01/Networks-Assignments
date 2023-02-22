# PA1 | UDP Client/Server File Transfer | Hampton Walker

This is my solution to PA1 for CSCI 4273 at CU Boulder.

## [Back Button](../README.md)

## Compiling & Usage

To start, `cd` into `clientDir`/`serverDir` and compile the file(s) with gcc (e.g. `gcc udp_server.c -o server` to have the executable serverr named as "server"). Firstly, start up the server with `./executable_name portno`, where executeble_name is the name of the executable and portno is any available, ephemeral port. To start the client, the usage is `./executable_name host portno`, where `portno` matches the server port and `host` is the ip address of the server (`localhost` if running both on the same machine).

## Client Commands available

- `get [filename]`: gets a file from the server and stores the file in the cwd of the client's execuatble.

- `put [filename]`: puts a file onto the server and stores the file in the cwd of the server's executable.

- `delete [filename]`: deletes a file from the server.

- `ls`: lists all available files in the server's cwd (output looks like `ls -la serverDir`)

- `exit`: kills the client with an exit message. Server stays active until user interrupts with `Ctrl+C`.

I was able to transfer a 1GB file in about 12-15 seconds between client and server and vice versa. Binary and text files were able to be transferred without data loss.

## Testing

We were given 3 files to test with: foo1-3. To make bigger files, I simply `cat`'ed `foo3` 10x times and wrote it to another file, and continued to do this until I had a 1GB file. I manually tested this assignment with the following method:

- `ls -l` my client directory to see the number of bytes in a file to test with
- connect client to the server
- `put` a file onto the server
- `ls` the server to make sure the number of bytes match up
- create a backup of the file I put onto the server
- `get` the file from the server
- `diff` the file I got with the backup and make sure there is no console output.

I could have written a script to do this for me, but this was before I learned a whole lot of bash. I wrote test scripts for the other assignments to automate my testing.

## Grade: 100/100
