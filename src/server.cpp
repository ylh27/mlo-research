// server.cpp
#include <arpa/inet.h>
#include <chrono>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <bits/stdc++.h>
#include <iostream>

#include "helper.h"
#include "server.h"

// #define DEBUG_SERVER 1

#define BACKLOG 10 // how many pending connections queue will hold

int server(std::string port, bool verbose, bool continuous, bool trivial) {
#ifdef DEBUG_SERVER
    std::cout << "port: " << port << std::endl;
#endif
    int sockfd; // listen on sock_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size = sizeof(their_addr);
    int yes = 1;
    std::string s;
    s.resize(INET6_ADDRSTRLEN);
    int rv;
    struct sigaction sa;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port.data(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
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

    // std::vector<unsigned> received;
    std::vector<std::pair<std::string, unsigned>> received;                                          // store received packets
    std::vector<std::pair<std::string, std::chrono::time_point<std::chrono::system_clock>>> clients; // store clients and their last received time

    std::chrono::time_point<std::chrono::system_clock> main_start;

    // main recv loop
    std::string start_str = START;
    std::string end_str = END;
    while ((numbytes = recvfrom(sockfd, buf.data(), MAXDATASIZE, 0, (struct sockaddr *)&their_addr, &sin_size)) > 0) {
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s.data(), sizeof(s));
        printf("server: got connection from %s\n", s.data());

        buf.resize(numbytes);

        // start signal
        if (buf.substr(0, strlen(START)) == start_str) {
            if (verbose)
                std::cout << "server: received start signal" << std::endl;

            if (clients.empty()) // start main clock
                main_start = std::chrono::system_clock::now();

            std::pair<std::string, std::chrono::time_point<std::chrono::system_clock>> client(s, std::chrono::system_clock::now());
            clients.push_back(client);
        }

        // end signal
        else if (buf.substr(sizeof(unsigned), strlen(END)) == end_str) {
            std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

            if (verbose)
                std::cout << "server: received end signal" << std::endl;

            // find client
            auto it = std::find_if(clients.begin(), clients.end(), [&](const auto &client) { return client.first == s; });
            if (it != clients.end()) {
                std::chrono::duration<double, std::milli> elapsed_time = end - it->second;

                unsigned recved;
                auto recv_it = std::find_if(received.begin(), received.end(), [&](const auto &pair) { return pair.first == s; });
                if (recv_it != received.end())
                    recved = recv_it->second + 1;
                else
                    recved = 0;
                unsigned expected = *(unsigned *)(buf.data() + sizeof(unsigned) + strlen(END)) + 1;

                std::cout << "server: received " << recved << " out of " << expected << std::endl;
                std::cout << "server: client " << s << " total time " << elapsed_time.count() << " ms" << std::endl;

                double bandwidth = recved * MAXDATASIZE / (elapsed_time.count() / 1000);
                std::cout << "server: client " << s << " bandwidth " << bandwidth << " B/s" << std::endl;

                clients.erase(it);

                if (clients.empty()) {
                    std::chrono::duration<double, std::milli> main_elapsed_time = end - main_start;

                    unsigned tot_recved = 0;
                    for (auto &pair : received)
                        tot_recved += pair.second + 1;
                    expected = *(unsigned *)buf.data() + 2;

                    std::cout << "server: total received " << recved << " out of " << expected << std::endl;
                    std::cout << "server: total time " << main_elapsed_time.count() << " ms" << std::endl;

                    double main_bandwidth = tot_recved * MAXDATASIZE / (main_elapsed_time.count()/ 1000);
                    std::cout << "server: total bandwidth " << main_bandwidth << " B/s" << std::endl;
                }
            } else {
                std::cerr << "server: client not found" << std::endl;
            }

            break;
        }

        /*if (buf.substr(sizeof(unsigned)) == END) {
            if (verbose) {
                std::cout << "total packets expected: " << *(unsigned *)buf.data() << std::endl;
                std::cout << "total packets received: " << received.front() + received.size() << std::endl;
            }
            break;
        }*/

        else {
            unsigned num = *(unsigned *)buf.data();
            if (verbose)
                std::cout << "server: packet: " << num << std::endl;

            // record received packets
            auto it = std::find_if(received.begin(), received.end(), [&](const auto &pair) { return pair.first == s; });
            if (it != received.end())
                it->second++;
            else
                received.push_back({s, 1});

            // flow control
            // switch to sender side kernel implementation in the future
            // send ack to client
            if (sendto(sockfd, buf.data(), sizeof(unsigned), 0, (struct sockaddr *)&their_addr, sin_size) == -1) {
                perror("sendto");
                exit(1);
            }
            if (verbose)
                std::cout << "server: sent ack: " << num << std::endl;

            // received
            /*if (received.empty())
                received.push_back(num);
            else if (num <= received.front() + 1) {
                if (num == received.front() + 1)
                    received.front() = num;
                while (received.size() > 1) {
                    if (received[1] == received.front() + 1) {
                        received.front() = received[1];
                        received.erase(received.begin() + 1);
                    }
                }
            } else if (num > received.front() + 1 && std::find(received.begin(), received.end(), num) == received.end()) {
                received.push_back(num);
                std::sort(received.begin(), received.end());
            }*/

#ifdef DEBUG_SERVER
            std::cout << "server: received '" << buf.substr(sizeof(unsigned)) << "'" << std::endl;
#endif

            // write output
            if (!trivial) {
                fp = fopen("output", "a");
                fwrite(buf.data() + sizeof(unsigned), 1, buf.size() - sizeof(unsigned), fp);
                fclose(fp);
            }
        }

        // send ack
        /*
        unsigned size = std::min((unsigned)(sizeof(unsigned) * received.size()), (unsigned)MAXDATASIZE);
        buf.resize(size);
        for (unsigned i = 0; i < size / sizeof(unsigned); i++)
            *((unsigned *)buf.data() + i) = received[i];
        sendto(sockfd, buf.data(), buf.size(), 0, (struct sockaddr *)&their_addr, sin_size);
        */

        buf.resize(MAXDATASIZE);
    }

    if (numbytes == -1) {
        perror("recv");
        exit(1);
    }

    close(sockfd);

    if (continuous)
        return server(port, verbose, continuous, trivial);

    return 0;
}
