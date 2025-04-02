// helper.cpp
# include "helper.h"

void sigchld_handler(int s)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
