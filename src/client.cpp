// client.cpp
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <array>
#include <iostream>

#include "client.h"
#include "helper.h"

// #define DEBUG_CLIENT 1

static bool verbose;
static int dispatch_pipe[2];
static std::vector<std::array<int, 2>> pipes;

static void master_callback(std::string file) {
    close(dispatch_pipe[0]); // close read end

    unsigned read = 0; // number of packets sent

    FILE *fp = fopen(file.data(), "rb");

    std::string buf;
    buf.resize(MAXDATASIZE);

    // read file
    size_t numbytes;
    while ((numbytes = fread(buf.data() + sizeof(unsigned), 1, MAXPAYLOAD, fp)) > 0) {
        *(unsigned *)buf.data() = read;
        read++;
        buf.resize(numbytes + sizeof(unsigned));

        buf.resize(MAXDATASIZE, ' '); // pad to MAXDATASIZE

#ifdef DEBUG_CLIENT
        std::cout << "writing to pipe: " << buf.substr(sizeof(sizeof(unsigned))) << std::endl;
#endif

        if (write(dispatch_pipe[1], buf.data(), buf.size()) < 0) {
            perror("write");
            exit(1);
        }
    }
    fclose(fp);

    if (verbose)
        std::cout << "client: entire file read" << std::endl;

    close(dispatch_pipe[1]); // close write end
}

static void dispatch_callback() {
    close(dispatch_pipe[1]); // close write end
    for (auto &pipe : pipes)
        close(pipe[0]); // close read end

    // main dispatch loop
    ssize_t numbytes;
    std::string buf;
    buf.resize(MAXDATASIZE);
    while ((numbytes = read(dispatch_pipe[0], buf.data(), MAXDATASIZE)) > 0) {
        if (numbytes != MAXDATASIZE) {
            std::cout << "client: datasize mismatch: " << numbytes << std::endl;
            continue;
        }

        write(pipes[rand() % pipes.size()][1], buf.data(), MAXDATASIZE);

        if (verbose)
            std::cout << "wrote " << *(unsigned *)buf.data() << " to pipe" << std::endl;
    }

    if (verbose)
        std::cout << "client: all packets dispatched" << std::endl;

    close(dispatch_pipe[0]); // close read end
    for (auto &pipe : pipes)
        close(pipe[1]); // close write end
}

static void send_callback(std::string ip, std::string port) {
    close(dispatch_pipe[0]);
    close(dispatch_pipe[1]);
    for (auto pipe = pipes.begin(); *pipe != pipes.back(); pipe++) {
        close((*pipe)[0]);
        close((*pipe)[1]);
    }

    int sockfd;
    ssize_t numbytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    close(pipes.back()[1]); // close write end

    if ((rv = getaddrinfo(ip.data(), port.data(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    if (verbose) {
        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
        printf("client: connecting to %s\n", s);
    }

    freeaddrinfo(servinfo); // all done with this structure

    std::string buf;
    buf.resize(MAXDATASIZE);

    bool start_sent = false;
    unsigned sent = 0; // number of packets sent

    // send data from pipe
    while ((numbytes = read(pipes.back()[0], buf.data(), MAXDATASIZE)) > 0) {
        if (numbytes != MAXDATASIZE) {
            std::cout << "client: datasize mismatch: " << numbytes << std::endl;
            continue;
        }

        if (!start_sent) { // send start signal
            std::string start_buf = START;
            start_buf.resize(MAXDATASIZE, ' ');

            if (sendto(sockfd, start_buf.data(), MAXDATASIZE, 0, p->ai_addr, p->ai_addrlen) == -1) {
                perror("send");
                exit(1);
            }

            if (verbose)
                std::cout << "client: sent start signal" << std::endl;

            start_sent = true;
        }

#ifdef DEBUG_CLIENT
        std::cout << "sending: " << buf << std::endl;
#endif
        if (sendto(sockfd, buf.data(), MAXDATASIZE, 0, p->ai_addr, p->ai_addrlen) == -1) {
            perror("send");
            exit(1);
        }
        sent++;

        if (verbose)
            std::cout << "client: sent packet: " << *(unsigned *)buf.data() << std::endl;
    }

    if (numbytes == -1) {
        perror("read");
        exit(1);
    }

    close(pipes.back()[0]); // close read end

    // send end signal
    buf = buf.substr(0, 4);
    buf += END;
    buf.resize(MAXDATASIZE, ' ');

    if (sendto(sockfd, buf.data(), MAXDATASIZE, 0, p->ai_addr, p->ai_addrlen) == -1) {
        perror("send");
        exit(1);
    }

    if (verbose)
        std::cout << "client: sent end signal" << std::endl;

    close(sockfd);
}

int client(std::vector<std::string> ips, std::string port, std::string file, std::string algorithm, bool verbose_) {
#ifdef DEBUG_CLIENT
    for (auto i : ips)
        std::cout << "ip: " << i << std::endl;
    std::cout << "port: " << port << std::endl;
    std::cout << "file: " << file << std::endl;
    std::cout << "algorithm: " << algorithm << std::endl;
#endif
    verbose = verbose_;

    std::vector<pid_t> children;
    struct sigaction sa;

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    enum { master, dispatch, send } type = master;

    // create dispatch
    if (pipe(dispatch_pipe) != 0) {
        perror("pipe");
        exit(1);
    }
    pid_t dispatch_pid = fork();
    if (dispatch_pid == 0) // child
        type = dispatch;

    // create children for send
    std::string ip;
    if (type == dispatch) { // only fork from dispatch
        while (!ips.empty()) {
            ip = ips.back();
            ips.pop_back();

            pipes.push_back({0, 0});
            if (pipe(pipes.back().data()) != 0) {
                perror("pipe");
                exit(1);
            }
            pid_t child = fork();
            if (child == 0) // child
            {
                type = send;
                break;
            } else // parent
            {
                children.push_back(child);
                continue;
            }
        }
    }

    switch (type) {
    case master:
        master_callback(file);
        break;

    case dispatch:
        dispatch_callback();
        break;

    case send:
        send_callback(ip, port);
        break;

    default: // unreachable
        std::cerr << "Error: unreachable code" << std::endl;
        exit(1);
    }

    // wait for all the children to exit before returning
    if (type == dispatch) {
        for (auto child : children) {
            int status;
            waitpid(child, &status, 0);
        }
    } else if (type == master) {
        int status;
        waitpid(dispatch_pid, &status, 0);
    }

    return 0;
}
