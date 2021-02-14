#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/**
 * @brief Sends a packet to the server that is connected via sfd
 * @param sfd The socket which should already be connected to the server.
 * @param buf The packet to send
 * @param len The size of the packet
 * 
 * \returns The total amount of bytes that were received or -1 for error
 */
ssize_t SendFull(int sfd, const void *buf, ssize_t len)
{
    ssize_t bytesSent = 0;
    while (1)
    {
        ssize_t result = send(sfd,
                              (const char *)buf + bytesSent,
                              len - bytesSent,
                              0);
        if (result == -1)
        {
            perror("SendFull(): ");
            return result;
        }

        if ((bytesSent += result) == len)
            return bytesSent;
    }
}

/**
 * @brief Receives data from server connected via sfd and fills buffer pointed 
 *        to buf until it receives len bytes or an orderly shutdown occurs
 * @param sfd The socket which should already be connected to the server.
 * @param buf The location where to store the data received from server
 * @param len The size of the buffer pointed to by buf
 * 
 * \returns The total amount of bytes that were received or -1 for error
 */
ssize_t RecvFull(int sfd, void *buf, ssize_t len)
{
    ssize_t bytesReceived = 0;
    while (1)
    {
        ssize_t result = recv(sfd,
                              (char *)buf + bytesReceived,
                              len - bytesReceived,
                              0);
        if (result == -1)
        {
            perror("RecvFull(): ");
            return result;
        }

        if (!result || (bytesReceived += result) == len)
            return bytesReceived;
    } 
}

int main(int argc, const char **argv)
{
    int connectionfd;
    int filefd = -1;

    if (argc < 4)
    {
        printf("USAGE: %s <HOSTNAME> <PORTNO> <FILE>\n", argv[0]);
        return -1;
    }
        
    // Get all available connections to this server using this service
    // This configuration assumes the server you are sending the HTTP request
    // to has port 80 open listening for TCP connections. 
    // If you are unsure if this will work, try running nmap <HOST> and seeing
    // what ports are open and what transport layer protocol is being used, and
    // configure the hints object correctly. 
    int errorCode;
    struct addrinfo *result;
    struct addrinfo hints = 
    { 
        // Use IPv4 or IPv6 protocol family/domain
        .ai_family   = AF_UNSPEC, 
        // Do not narrow down any further with flags
        .ai_flags    = 0,
        // Use any protocol for the socket
        .ai_protocol = 0,
        // Use TCP (connection-oriented) sockets
        .ai_socktype = SOCK_STREAM
    };
    if ((errorCode = getaddrinfo(argv[1], argv[2], &hints, &result)))
    {
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(errorCode));
        return -1;
    }

    // Iterate through linked list of socket addresses and create a connection
    // endpoint
    struct addrinfo *addr = result; 
    for (; addr; addr = addr->ai_next)
    {
        // Attempt to setup the socket with the properties from address node
        if ((connectionfd = socket(addr->ai_family,
                                   addr->ai_socktype,
                                   addr->ai_protocol)) == -1)
            continue;

        // Attempt to establish the UDP connection with this socket 
        if (!connect(connectionfd, addr->ai_addr, addr->ai_addrlen))
            break;
        
        close(connectionfd);
    }
    if (!addr)
    {
        freeaddrinfo(result);
        fprintf(stderr, "Failed to create an endpoint for communication!\n");
        return -1;
    }

    // Perform reverse DNS lookup to obtain the name of the server and port we
    // are sending an HTML GET request to and print it
    char hostBuf[NI_MAXHOST];
    char servBuf[NI_MAXSERV];
    int status = getnameinfo(addr->ai_addr,
                             addr->ai_addrlen,
                             hostBuf,
                             NI_MAXHOST,
                             servBuf,
                             NI_MAXSERV,
                             0);
    if (status)
    {
        fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(status));
        close(connectionfd);
        freeaddrinfo(result);
        return -1;
    }
    fprintf(stdout,
            "Sending to host %s on port %hu\n",
            hostBuf,
            ntohs(((struct sockaddr_in *)addr->ai_addr)->sin_port));

    // Create a HTTP 1.1 GET request to obtain the file stored on the server
    // We need to add the "Connection: close" into the HTTP request header
    // because of HTTP 1.1's persistant connection which will keep the 
    // connection alive after closing just in case of a reconnection. If we
    // abruptly close the connection without telling the server we don't want
    // to use the persistent connection, the server sees the second carriage
    // return and line feed as another command if the neither the server or 
    // client closes the connection and returns a bad request
    char httpRequest[0x100];
    snprintf(httpRequest,
             sizeof httpRequest,
             "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
             argv[3],
             argv[1]);

    // Attempt to deliver the HTTP request packet to the server
    if (!SendFull(connectionfd, httpRequest, strlen(httpRequest) + 1))
    {
        freeaddrinfo(result);
        close(connectionfd);
        return -1;
    }

    char buf[0x400] = { 0 };
    while (1)
    {
        // Monitor the connection socket to prevent a blocking syscall from 
        // holding the program.
        fd_set readfds;
        FD_SET(connectionfd, &readfds);
        
        // Wait for a maximum of 1 seconds for socket to be ready to receive
        // data from server
        struct timeval timer;
        timer.tv_sec = 1;
        timer.tv_usec = 0;

        // Wait for the socket file descriptor to be ready
        if (select(connectionfd + 1, &readfds, NULL, NULL, &timer) == -1)
        {
            perror("select()");
            if (fcntl(filefd, F_GETFD) != -1)
                close(filefd);
            close(connectionfd);
            return -1;
        }

        // Check if the socket file descriptor is ready for input so recv will
        // not block when called
        if (!FD_ISSET(connectionfd, &readfds))
        {
            fprintf(stderr, "Socket fd was not set");
            break;
        }

        // Receive the data from the server
        ssize_t len;
        if ((len = RecvFull(connectionfd, buf, sizeof buf)) <= 0)
            break;

        /**
         * TODO: Add HTML header parsing to only create and write file if HTML
         *       response was OK
        */

        // Create the file if it has not been opened yet
        if (fcntl(filefd, F_GETFD) != -1)
        {
            // Write the buffer contents to the file
            if (write(filefd, buf, len) == -1)
            {
                perror("write()");
                freeaddrinfo(result);
                close(connectionfd);
                close(filefd);
                return -1;
            }
        }
        else if ((filefd = open(argv[3], O_CREAT | O_RDWR | O_TRUNC)) == -1)
        {
            perror("open()");
            freeaddrinfo(result);
            close(connectionfd);
            return -1;
        }

        for (ssize_t i = 0; i < len; i++)
            putc(buf[i], stdout);
    }
    
    freeaddrinfo(result);
    close(connectionfd);
    close(filefd);
    return 0;
}
