// helper.h
#ifndef HELPER_H
#define HELPER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXDATASIZE 1472 // max number of bytes we can get at once
#define MAXPAYLOAD (MAXDATASIZE - sizeof(unsigned))
#define START "\r\n\r\nSTART\r\n\r\n"
#define END "\r\n\r\nEOF\r\n\r\n"

void sigchld_handler(int);

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *);

#endif // HELPER_H
