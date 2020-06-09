#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <iostream>
#include <map>
#include <vector>

#include "err.h"
#include "my_time.h"
#include "network.h"
#include "parser.h"
#include "socket_manager.h"

using namespace std;

bool finish_program = false;

// Program constants.
const int default_package_size = 4000;
const string default_radio_name = "Unknown";

void signalHandler( __attribute__((unused))int signum ) {
    finish_program = true;
}

void print_usage() {
    cerr << "Usage: ./radio-proxy -h host -r resource -p port [-m yes|no] [-t timeout] " <<
            "[-P agent_port [-B multicast_address] [-T agent_timeout]" << endl;
    exit(1);
}

struct Client {
    timeval last_message;
    sockaddr_in sock_address;

    Client() {}

    Client(sockaddr_in sock_address) : sock_address(sock_address) {
        last_message = time_now();
    }

    void update_time() {
        last_message = time_now();
    }
};

// Checks which clients are active and remove the inactive ones.
void check_alive(map<string, Client> &client_map, int timeout) {
    vector<string> to_remove;
    for (auto it = client_map.begin(); it != client_map.end(); it++) {
        if (microseconds_passed_from(it->second.last_message) > timeout * 1000000) {
            to_remove.push_back(it->first);
        }
    }

    for (auto rem : to_remove) {
        client_map.erase(rem);
    }
}

// Sends message to all clients.
void write_to_all(map<string, Client> &client_map, int sock, string &data, unsigned type) {
    for (auto client : client_map) {
        udp_write(sock, data, &client.second.sock_address, type);
    }
}

// Initializes connection with server and reads header.
// Returns a tuple containing a beginning of stream, radio name and metaint.
tuple<string, string, int> initialize_connection(proxy_params &params, int sock) {
    ssize_t rcv_len;

    // Prepare and send the GET request.
    string message = "GET " + params.resource + " HTTP/1.1\r\n";
    message += "Host: " + params.host + "\r\n";
    message += "Icy-MetaData:" + to_string(params.metadata) + "\r\n\r\n";

    tcp_write(sock, message);

    string header = "";

    // Read the header.
    while (header.find("\r\n\r\n") == string::npos) {
        int radio_in = sock, agent_in = -1;
        int events = custom_select(radio_in, agent_in, time_left(time_now(), params.timeout));

        if (events == 0) {
            fatal("Connection lost");
        } else if (events == -1) {
            finish_program = true;
        }

        string read_part;
        rcv_len = tcp_read(sock, read_part);

        if (rcv_len < 0) {
            syserr("read");
        } else if (rcv_len == 0) {
            fatal("Connection terminated");
        }

        header += read_part;
    }

    int header_len = header.find("\r\n\r\n");

    transform(header.begin(), header.end(), header.begin(), ::toupper);

    if (header.find("ICY 200 OK") != 0 &&
        header.find("HTTP/1.0 200 OK") != 0 &&
        header.find("HTTP/1.1 200 OK") != 0) {
        fatal("Response status differs from 200 OK");
    }

    // Get metaint and radio name.
    int metaint = default_package_size;
    const string METAINT = "ICY-METAINT:";
    unsigned long metaint_location = header.find(METAINT);
    if (params.metadata && metaint_location != string::npos) {
        metaint = atoi(header.c_str() + metaint_location + METAINT.size());
    } else if (params.metadata) {
        params.metadata = false;
    } else if (metaint_location != string::npos) {
        fatal("Server forces metadata");
    }

    string radio_name = "";
    const string NAME = "ICY-NAME:";
    unsigned long name_location = header.find(NAME);
    if (name_location == string::npos) {
        radio_name = default_radio_name;
    } else {
        for (int i = name_location + NAME.size(); header[i] != '\r'; i++) {
            radio_name += header[i];
        }
    }

    // Calculate the beginning of the stream.
    string response_beginning = "";
    for (int i = header_len + 4; i < (int)header.size(); i++) {
        response_beginning += header[i];
    }

    return make_tuple(response_beginning, radio_name, metaint);
}

// Decides if a package should be send to the clients.
// Depending on the decision, send it and modifies package string, or does no changes.
// Gets information if current data is metadata or audio.
void send_package_if_necessary(string &package, int metaint, bool &metadata_now, proxy_params &params,
        map<string, Client> &client_map, int agent_sock, string &last_metadata) {
    // Repeats the action while it's possible that an audio package/matadata piece is ready.
    while ((int)package.size() >= metaint || metadata_now) {
        int ps = package.size();
        if (metadata_now) {
            if (!params.metadata) {
                metadata_now = false;
            } else if (ps == 0) {
                break;
            } else {
                unsigned char first = package[0];
                if (ps <= 16 * first)
                    break;

                string metadata(1, package[0]);
                string new_package = "";

                for (int i = 1; i <= 16 * first; i++) {
                    metadata += package[i];
                }
                for (int i = 16 * first + 1; i < ps; i++) {
                    new_package += package[i];
                }

                package = new_package;
                metadata_now = false;

                if (params.agent_active) {
                    if (first != 0) {
                        write_to_all(client_map, agent_sock, metadata, METADATA);
                        last_metadata = metadata;
                    }
                } else {
                    fwrite(metadata.c_str(), sizeof(char), metadata.size(), stderr);
                }
            }
        } else {
            string new_package = "";
            for (int i = metaint; i < ps; i++)
                new_package += package[i];

            package.resize(metaint);
            if (params.agent_active) {
                write_to_all(client_map, agent_sock, package, AUDIO);
            } else {
                fwrite(package.c_str(), sizeof(char), metaint, stdout);
            }

            package = new_package;
            metadata_now = true;
        }
    }
}

// Reads a message from any agent and responds in a right way.
void agent(int sock, map<string, Client> &client_map, string &radio_name, string last_metadata) {
    sockaddr_in sender_address;
    string message;
    uint16_t type;

    ssize_t rcv_len = udp_read(sock, message, &sender_address, type);
    string char_address = get_address_string(sender_address);

    if (rcv_len >= 0) {
        if (type == DISCOVER) {
            udp_write(sock, radio_name, &sender_address, IAM);
            if (last_metadata != "") {
                udp_write(sock, last_metadata, &sender_address, METADATA);
            }
            client_map[char_address] = Client(sender_address);
        } else if (type == KEEPALIVE) {
            if (client_map.count(char_address)) {
                client_map[char_address].update_time();
            }
        } else {
            cerr << "Unknown type\n";
        }
    } else {
        cerr << "Incorrect UDP header\n";
    }
}

// Main proxy functionality.
void proxy(proxy_params &params) {
    int sock = create_connected_socket(params.host, params.port);
    ssize_t rcv_len;

    string package, radio_name;
    int metaint;

    tie(package, radio_name, metaint) = initialize_connection(params, sock);

    // Initiates the agent socket.
    int agent_sock = -1;
    ip_mreq ip_mreq;
    if (params.agent_active) {
        auto res = create_multicast_socket(params.multicast_address, params.agent_port);
        agent_sock = res.first;
        ip_mreq = res.second;
    }
    map<string, Client> client_map;


    bool metadata_now = false;
    timeval last_stream_package = time_now();
    string last_metadata = "";

    // Main program loop.
    while (!finish_program) {

        int radio_in = sock, agent_in = agent_sock;
        int events = custom_select(radio_in, agent_in, time_left(last_stream_package, params.timeout));

        if (events == 0) {
            fatal("Connection terminated or lost");
        } else if (events == -1) {
            finish_program = true;
        }

        // If a message from server came, read it and take action.
        if (radio_in == 1) {
            string read_string;
            rcv_len = tcp_read(sock, read_string);

            if (rcv_len < 0) {
                syserr("read");
            } else if (rcv_len == 0) {
                fatal("Connection terminated");
            }

            package += read_string;
            last_stream_package = time_now();

            send_package_if_necessary(package, metaint, metadata_now, params, client_map, agent_sock, last_metadata);
        }

        // If a message from a client came, read it and respond.
        if (agent_in == 1 && params.agent_active) {
            agent(agent_sock, client_map, radio_name, last_metadata);
        } else if (params.agent_active) {
            check_alive(client_map, params.timeout);
        }
    }

    if (params.multicast_address != "" && params.agent_active) {
        close_multicast_socket(agent_sock, ip_mreq);
    } else if (params.agent_active) {
        close_socket(agent_sock);
    }
    close_socket(sock);

}

int main(int argc, char *argv[]) {
    signal(SIGINT, signalHandler);

    proxy_params params = parse_proxy_params(argc, argv, print_usage);

    proxy(params);
}


