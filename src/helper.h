// helper.h
#ifndef HELPER_H
#define HELPER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define END "\r\n\r\nEOF\r\n\r\n"

void sigchld_handler(int);

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *);

#endif // HELPER_H
