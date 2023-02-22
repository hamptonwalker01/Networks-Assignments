# PA2 | TCP Web Server | Hampton Walker

This is my solution to PA2 for CSCI 4273 - Network Systems. For this assignment, we had to serve up all files within the [www](./www/) directory. Only GET requests with HTTP version 1.0/1.1 were allowed for the assignment. We were also limited to the content types that our server would serve to the usual suspects (html, css, txt, js, png, jpg, gif). Standard error messages were expected to be returned with proper status codes.

## Compiling & Usage

Compile `server.c` with gcc as usual. Once you have the executable, make sure it is in the same directory as the [www](./www/), or else you will not be able to see the server once started. The server expects a single extra argument besides the executable name, which is an ephemeral port number. Once started, you can navigate to `127.0.0.1:portno` in your browser to see the website given.

## Testing

I wrote a test script for this assignment to make sure I was testing certain edge cases properly and returning proper error codes. The script [testscript.sh](./testscript.sh) needs a single argument, which is the port that your server is running on. It then uses netcat to test various requests and make sure we either get the expected page or proper error message/status code back.

As a side note, after I made this script, I distributed it to a few friends in the class. My script caught errors in several other student's code - namely some students' servers would hang on an empty request. My classmates seemed very grateful for my test script, so I was glad I was able to help other people succeed besides myself.

## Grade: 85/100

I did not do as good as I expected for this assignment, because I only called `read()` once when accepting a request instead of looping until I saw a CLRF or timed out. That error lost me 10 points, and I lost another 5 points for returning a 405 error on a HELO request instead of a 400 error. These issues were simply due to my parsing methods and logic of when to throw errors, which was fixed for PA3.
