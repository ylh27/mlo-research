#include <iostream>
#include <string>
#include <vector>

#include "client.hpp"
#include "server.hpp"

// #define DEBUG 1

#define PORT 5001

struct arg
{
    std::string mode;
    std::vector<std::string> ips;
    std::string port;
    std::string file;
    std::string algorithm;
};

void print_usage(char *argv)
{
    std::cout << "Usage: " << argv << " [-s|-c] [-i <ip>] [-p <port>] [-f <file>] [-a <algorithm>]" << std::endl;
}

int main(int argc, char *argv[])
{
    struct arg arg;

    // loop thru argv
    for (int i = 1; i < argc; i++)
    {
        if (std::string(argv[i]) == "-s" && arg.mode == "")
        {
            arg.mode = "server";
        }
        else if (std::string(argv[i]) == "-c" && arg.mode == "")
        {
            arg.mode = "client";
        }
        else if (std::string(argv[i]) == "-i")
        {
            arg.ips.push_back(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-p" && arg.port == "")
        {
            arg.port = argv[i + 1];
            i++;
        }
        else if (std::string(argv[i]) == "-f" && arg.file == "")
        {
            arg.file = argv[i + 1];
            i++;
        }
        else if (std::string(argv[i]) == "-a" && arg.algorithm == "")
        {
            arg.algorithm = argv[i + 1];
            i++;
        }
    }

#ifdef DEBUG
    std::cout << "mode: " << arg.mode << std::endl;
    for (auto ip : arg.ips)
        std::cout << "ip: " << ip << std::endl;
    std::cout << "port: " << arg.port << std::endl;
    std::cout << "file: " << arg.file << std::endl;
    std::cout << "algorithm: " << arg.algorithm << std::endl;
#endif

    // check neccessary args set
    if (arg.mode == ""                                                                           // no mode
        || (arg.mode == "server" && (false))                                                     // server mode
        || (arg.mode == "client" && (arg.ips.empty() || arg.file == "" || arg.algorithm == ""))) // client mode
    {
        print_usage(argv[0]);
        return 1;
    }

    // set default port
    if (arg.port == "")
    {
        arg.port = std::to_string(PORT);
    }

    if (arg.mode == "server")
    {
        return server(arg.port);
    }
    else if (arg.mode == "client")
    {
        return client(arg.ips, arg.port, arg.file, arg.algorithm);
    }
    else
    {
        print_usage(argv[0]);
        return 1;
    }

    return 1;
}
