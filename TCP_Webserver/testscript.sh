#!/bin/bash

PORT=$1

function output() {
    if [[ $1 -eq 0 ]]; then
        echo "Successful"
    else
        echo "Error"
    fi
}

echo -e "\n----TESTING ERRORS-----\N"

echo -e "\n400 ERRORS\n"
echo -e "\nTesting Empty Request"
echo -e "" | nc localhost $PORT | grep "400" &> /dev/null
output $?

echo -e "\nTesting No URI & HTTP"
echo -en "GET \r\n\r\n" | nc localhost $PORT | grep "400" &> /dev/null
output $?

echo -e "\nTesting No HTTP"
echo -en "GET / \r\n\r\n" | nc localhost $PORT | grep "400" &> /dev/null
output $?

echo -e "\nTesting No URI"
echo -en "GET HTTP/1.0\r\n\r\n" | nc localhost $PORT | grep "400" &> /dev/null
output $?

echo -e "\nTesting No Spaces"
echo -en "GET/HTTP/1.0\r\n\r\n" | nc localhost $PORT | grep "400" &> /dev/null
output $?

echo ""
echo "Testing no \r\n\r\n"
echo -en "GET / HTTP/1.0" | nc localhost $PORT | grep "400" &> /dev/null
output $?

echo -e "\nTesting no Host in HTTP/1.1"
echo -en "GET / HTTP/1.1\r\n\r\n" | nc localhost $PORT | grep "400" &> /dev/null
output $?

echo -e "\nTesting Content Type Not Allowed"
echo -en "GET /files/welcome.html~ HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "400" &> /dev/null
output $?

echo -e "\n403 ERRORS\n"
echo -e "\nTesting Malicious Request"
echo -en "GET /../test HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "403" &> /dev/null
output $?

echo -e "\n404 ERRORS\n"
echo -e "\nTesting File Not Found"
echo -en "GET /does/not/exist/ HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "404" &> /dev/null
output $?

echo -e "\nTesting File Not Found Again"
echo -en "GET /does/not/exist HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "404" &> /dev/null
output $?

echo -e "\n405 ERRORS\n"
echo -e "\nTesting METHOD!=GET"
echo -en "POST / HTTP/1.0\r\n\r\n" | nc localhost $PORT | grep "405" &> /dev/null
output $?

echo -e "\n505 ERRORS\n"
echo -e "\nTesting Wrong HTTP Version"
echo -en "GET /does/not/exist/ HTTP/1.2\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "505" &> /dev/null
output $?

echo -e "\n----TESTING FILES-----\n"
echo -e "\nTesting get index.html"
echo -en "GET / HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "200" &> /dev/null
output $?

echo -e "\nTesting get jquery"
echo -en "GET /jquery-1.4.3.min.js HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "200" &> /dev/null
output $?

echo -e "\nTesting get /css/style.css"
echo -en "GET /css/style.css HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "200" &> /dev/null
output $?

echo -e "\nTesting get /graphics/gif.gif"
echo -en "GET /graphics/gif.gif HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "200" &> /dev/null
output $?

echo -e "\nTesting get /images/apple_ex.png"
echo -en "GET /images/apple_ex.png HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "200" &> /dev/null
output $?

echo -e "\nTesting get /images/wine3.jpg"
echo -en "GET /images/apple_ex.png HTTP/1.1\r\n\Host: localhost\r\n\r\n" | nc localhost $PORT | grep "200" &> /dev/null
output $?