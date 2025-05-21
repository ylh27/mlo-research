// route.cpp
#include "route.h"
#include "client.h"
#include "server.h"

// #define DEBUG 1

#define PORT 5001

int main(int argc, char *argv[]) {
    struct arg arg;

    // loop thru argv
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-s" && arg.mode == "") {
            arg.mode = "server";
        } else if (std::string(argv[i]) == "-c" && arg.mode == "") {
            arg.mode = "client";
        } else if (std::string(argv[i]) == "-i") {
            arg.ips.push_back(argv[i + 1]);
            i++;
        } else if (std::string(argv[i]) == "-p" && arg.port == "") {
            arg.port = argv[i + 1];
            i++;
        } else if (std::string(argv[i]) == "-f" && arg.file == "") {
            arg.file = argv[i + 1];
            i++;
        } else if (std::string(argv[i]) == "-a" && arg.algorithm == "") {
            arg.algorithm = argv[i + 1];
            i++;
        } else if (std::string(argv[i]) == "-v") {
            arg.verbose = true;
        } else if (std::string(argv[i]) == "-d") {
            arg.continuous = true;
        } else if (std::string(argv[i]) == "-t") {
            arg.trivial = true;
            arg.size = std::stoul(argv[i + 1]);
            i++;
        } else {
            print_usage(argv[0]);
            return 1;
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
    if (arg.mode == ""                                                                                                        // no mode
        || (arg.mode == "server" && (false))                                                                                  // server mode
        || (arg.mode == "client" && (arg.ips.empty() || (arg.file == "" && !arg.trivial) || (arg.size == 0 && arg.trivial)))) // client mode
    {
        print_usage(argv[0]);
        return 1;
    }

    // set default port
    if (arg.port == "") {
        arg.port = std::to_string(PORT);
    }

    if (arg.mode == "server") {
        return server(arg.port, arg.verbose, arg.continuous);
    } else if (arg.mode == "client") {
        return client(arg.ips, arg.port, arg.file, arg.algorithm, arg.verbose, arg.trivial, arg.size);
    } else {
        print_usage(argv[0]);
        return 1;
    }

    return 1;
}
