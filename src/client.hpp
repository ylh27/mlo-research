#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <vector>
#include <array>

#include "helper.hpp"

#define DEBUG_CLIENT 1

#define MAXDATASIZE 1472 // max number of bytes we can get at once

int client(std::vector<std::string> ips, std::string port, std::string file, std::string algorithm)
{
#ifdef DEBUG_CLIENT
    for (auto i : ips)
        std::cout << "ip: " << i << std::endl;
    std::cout << "port: " << port << std::endl;
    std::cout << "file: " << file << std::endl;
    std::cout << "algorithm: " << algorithm << std::endl;
#endif
    int sockfd;
    ssize_t numbytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    std::vector<pid_t> children;
    bool master = true;
    struct sigaction sa;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    std::vector<std::array<int, 2>> pipes;
    std::string ip;
    while (!ips.empty())
    {
        ip = ips.back();
        ips.pop_back();

        pipes.push_back({0, 0});
        if (pipe(pipes.back().data()) != 0)
        {
            perror("pipe");
            exit(1);
        }

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

    if (!master)
    {
        close(pipes.back()[1]); // close write end

        if ((rv = getaddrinfo(ip.data(), port.data(), &hints, &servinfo)) != 0)
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and connect to the first we can
        for (p = servinfo; p != NULL; p = p->ai_next)
        {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) == -1)
            {
                perror("client: socket");
                continue;
            }

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
            {
                close(sockfd);
                perror("client: connect");
                continue;
            }

            break;
        }

        if (p == NULL)
        {
            fprintf(stderr, "client: failed to connect\n");
            return 2;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                  s, sizeof s);
        printf("client: connecting to %s\n", s);

        freeaddrinfo(servinfo); // all done with this structure

        std::string buf;
        buf.resize(MAXDATASIZE);

        // send data from pipe
        while ((numbytes = read(pipes.back()[0], buf.data(), MAXDATASIZE)) > 0)
        {
            buf.resize(numbytes);
#ifdef DEBUG_CLIENT
            std::cout << "sending: " << buf << std::endl;
#endif
            if (send(sockfd, buf.data(), buf.size(), 0) == -1)
            {
                perror("send");
                exit(1);
            }
            buf.resize(MAXDATASIZE);
        }

        if (numbytes == -1)
        {
            perror("read");
            exit(1);
        }

        close(pipes.back()[0]); // close read end
        close(sockfd);
    }
    else
    {
        for (auto &pipe : pipes)
            close(pipe[0]); // close read end

        FILE *fp = fopen(file.data(), "rb");
        if (fp == NULL)
        {
            perror("fopen");
            for (auto &pipe : pipes)
                close(pipe[1]); // close write end
            for (auto child : children)
                kill(child, SIGKILL);
            exit(1);
        }

        std::string buf;
        buf.resize(MAXDATASIZE);

        // read file
        while ((numbytes = fread(buf.data(), 1, MAXDATASIZE, fp)) > 0)
        {
            buf.resize(numbytes);

#ifdef DEBUG_CLIENT
            std::cout << "writing to pipe: " << buf << std::endl;
#endif

            write(pipes[rand() % pipes.size()][1], buf.data(), buf.size());
            // TODO: change to proper algorithm

            buf.resize(MAXDATASIZE);
        }

        // delay
        sleep(1);
        buf = END;
        write(pipes[rand() % pipes.size()][1], buf.data(), buf.size());
        // TODO: change to proper algorithm

        for (auto &pipe : pipes)
            close(pipe[1]); // close write end
        fclose(fp);
    }

    return 0;
}
