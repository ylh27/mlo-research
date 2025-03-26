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
#include <chrono>

#include "helper.hpp"

// #define DEBUG_CLIENT 1

#define MAXDATASIZE 1472 // max number of bytes we can get at once
#define MAXPAYLOAD (MAXDATASIZE - sizeof(unsigned))

int client(std::vector<std::string> ips, std::string port, std::string file, std::string algorithm, bool verbose)
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
    int status;
    struct sigaction sa;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

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
            if (sendto(sockfd, buf.data(), buf.size(), 0, p->ai_addr, p->ai_addrlen) == -1)
            {
                perror("send");
                exit(1);
            }
            buf.clear();
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
    { // master

        for (auto &pipe : pipes)
            close(pipe[0]); // close read end

        unsigned sent = 0; // number of packets sent

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

        std::chrono::time_point<std::chrono::system_clock> start, end;
        start = std::chrono::system_clock::now();
        // Start timer as soon as master finishes spawning children

        // read file
        while ((numbytes = fread(buf.data() + sizeof(unsigned), 1, MAXPAYLOAD, fp)) > 0)
        {
            *(unsigned *)buf.data() = sent;
            sent++;
            buf.resize(numbytes + sizeof(unsigned));

#ifdef DEBUG_CLIENT
            std::cout << "writing to pipe: " << buf.substr(sizeof(sizeof(unsigned))) << std::endl;
#endif

            int i = rand() % pipes.size();

            while (buf.size() < MAXDATASIZE)
                buf += " ";

            write(pipes[i][1], buf.data(), buf.size());

            std::cout << "[" << i << "] packet: " << sent << std::endl; // print which child and packet number
            // TODO: change to proper algorithm

            buf.resize(MAXDATASIZE);
        }
        fclose(fp);

        // TODO: change to proper algorithm
        // Algorithm start
        // end signal

        *(unsigned *)buf.data() = sent;
        buf.resize(sizeof(unsigned));
        buf += END;
        std::cout << "total packets: " << sent << std::endl;
        write(pipes[rand() % pipes.size()][1], buf.data(), buf.size());

        for (auto &pipe : pipes)
            close(pipe[1]); // close write end

        // Waits for all the children to exit before returning
        while (wait(&status) > 1)
            ;

        // End Timer Logic Here
        end = std::chrono::system_clock::now();
        std::chrono::duration<double, std::milli> elapsed_milliseconds = end - start;
        std::cout << "elapsed time: " << elapsed_milliseconds.count() << "ms\n";
    }

    return 0;
}
