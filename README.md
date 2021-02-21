[![Build status](https://ci.appveyor.com/api/projects/status/js3d2hnyf1u0papk?svg=true)](https://ci.appveyor.com/project/e7ite/simplehttpclient)

# SimpleHTTPClient
Basic Linux HTTP client which can send and receive HTTP 1.1 messages. It will make a GET request with whaterver file specified in first argument. Generally the port number will be 80 because that is the port number is reserved for HTTP. The HTTP response from the server will be sent to `stdout`.

## Usage
    ./simple_http_client.out <HOSTNAME> <PORTNO> <FILE>

## Build Instructions
1. Install build-essential on your package manager 
2. Run make
3. Run ./simple_http_client.out

## Preview
![Preview](/preview.gif)
