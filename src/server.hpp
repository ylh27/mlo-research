#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>

#include <iostream>
#include <string>
#include <vector>

#include "helper.hpp"

// #define DEBUG_SERVER 1

#define BACKLOG 10 // how many pending connections queue will hold
#define MAXDATASIZE 1472

int server(std::string port)
{
#ifdef DEBUG_SERVER
    std::cout << "port: " << port << std::endl;
#endif
    int sockfd; // listen on sock_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    struct sigaction sa;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port.data(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    std::string buf;
    ssize_t numbytes;

    FILE *fp;
    fp = fopen("output", "w");
    fclose(fp);

    printf("server: waiting for connections...\n");

    buf.resize(MAXDATASIZE);

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s));
    printf("server: got connection from %s\n", s);

    // main recv loop
    while ((numbytes = recvfrom(
                sockfd, buf.data(), MAXDATASIZE, 0,
                (struct sockaddr *)&their_addr, &sin_size)) > 0)
    {
        buf.resize(numbytes);

        if (buf.substr(sizeof(unsigned)) == END)
        {
            std::cout << "total packets expected: " << *(unsigned *)buf.data() << std::endl;
            break;
        }

        std::cout << "server: packet: " << *(unsigned *)buf.data() << std::endl;
#ifdef DEBUG_SERVER
        std::cout << "server: received '" << buf.substr(sizeof(unsigned)) << "'" << std::endl;
#endif

        fp = fopen("output", "a");
        fwrite(buf.data() + sizeof(unsigned), 1, buf.size() - sizeof(unsigned), fp);
        fclose(fp);

        buf.resize(MAXDATASIZE);
    }

    if (numbytes == -1)
    {
        perror("recv");
        exit(1);
    }

    close(sockfd);

    return 0;
}
