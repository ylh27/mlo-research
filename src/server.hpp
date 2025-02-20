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

#include <iostream>
#include <string>
#include <vector>

#include "helper.hpp"

#define DEBUG_SERVER 1

#define BACKLOG 10 // how many pending connections queue will hold
#define MAXDATASIZE 1472

int server(std::string port)
{
#ifdef DEBUG_SERVER
    std::cout << "port: " << port << std::endl;
#endif
    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    std::vector<pid_t> children;
    bool master = true;
    struct sigaction sa;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
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

        // spwan child
        pid_t child = fork();
        if (child == 0) // child
        {
            master = false;
            break;
        }
        else // parent
        {
            children.push_back(child);
            continue;
        }
    }

    if (p == NULL && !master)
    {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    std::string buf;
    ssize_t numbytes;

    if (!master)
    {
        if (listen(sockfd, BACKLOG) == -1)
        {
            perror("listen");
            exit(1);
        }

        printf("server: waiting for connections...\n");

        buf.resize(MAXDATASIZE);

        while (1) // main accept() loop
        {
            sin_size = sizeof their_addr;
            new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
            if (new_fd == -1)
            {
                perror("accept");
                continue;
            }

            inet_ntop(their_addr.ss_family,
                      get_in_addr((struct sockaddr *)&their_addr),
                      s, sizeof s);
            printf("server: got connection from %s\n", s);
        }

        while ((numbytes = recv(sockfd, buf.data(), MAXDATASIZE - 1, 0)) > 0)
        {
            buf.resize(numbytes);
            std::cout << "server: received '" << buf << "'" << std::endl;
            buf.resize(MAXDATASIZE);
        }

        if (numbytes == -1)
        {
            perror("recv");
            exit(1);
        }

        close(sockfd);
    }
    else
    {
    }

    return 0;
}
