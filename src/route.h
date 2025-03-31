// route.h
#ifndef ROUTE_H
#define ROUTE_H

#include <vector>
#include <string>
#include <iostream>

struct arg
{
    std::string mode;
    std::vector<std::string> ips;
    std::string port;
    std::string file;
    std::string algorithm;
    bool verbose = false;
    bool continuous = false;
};

void print_usage(char *argv)
{
    std::cout << "Usage: " << argv << " [-s|-c] [-i <ip>] [-p <port>] [-f <file>] [-a <algorithm>] [-v] [-d]" << std::endl;
}

#endif // ROUTE_H
