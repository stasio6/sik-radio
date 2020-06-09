#ifndef DUZE_PARSER_H
#define DUZE_PARSER_H

#include <string>

struct proxy_params {
    std::string host;
    std::string resource;
    int port;
    int metadata;
    int timeout;

    int agent_port;
    std::string multicast_address;
    int agent_timeout;
    bool agent_active;
};

struct client_params {
    std::string host;
    int port;
    int control_port;
    int timeout;
};

// Parse given radio-proxy params, returning them in a dedicated struct.
proxy_params parse_proxy_params(int argc, char *argv[], void (*print_usage)());

// Parse given client-proxy params, returning them in a dedicated struct.
client_params parse_client_params(int argc, char *argv[], void (*print_usage)());

#endif //DUZE_PARSER_H
