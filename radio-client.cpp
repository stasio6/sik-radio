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

// Program constants.
const int keepalive_frequency = 3500;

bool finish_program = false;

void signalHandler( __attribute__((unused))int signum ) {
    finish_program = true;
}

void print_usage() {
    cerr << "Usage: ./radio-client -H host -P port -p control_port [-T timeout]" << endl;
    exit(1);
}

struct Radio {
    string name;
    string address;
    timeval last_message;
    sockaddr_in sock_address;

    Radio() {}

    Radio(string reply, string address, sockaddr_in sock_address)
        : name(reply), address(address), sock_address(sock_address) {
        last_message = time_now();
    }

    void update_time() {
        last_message = time_now();
    }
};

// Checks which radios are active and remove the inactive ones.
// While removing radios, updates the cursor position.
bool check_alive(map<string, Radio> &radio_map, bool &telnet_update_needed, string &active, int timeout, int &cursor) {
    vector<string> to_remove;
    int row = 2;
    for (auto it = radio_map.begin(); it != radio_map.end(); it++) {
        if (microseconds_passed_from(it->second.last_message) > timeout * 1000000) {
            to_remove.push_back(it->first);
            telnet_update_needed = true;
            if (row <= cursor)
                cursor--;
        }
        row++;
    }

    for (auto rem : to_remove) {
        radio_map.erase(rem);
    }

    if (radio_map.find(active) == radio_map.end()) {
        active = "";
        return false;
    }

    return true;
}

// Updates the menu in the controlling telnet.
void send_update_to_telnet(map<string, Radio> &radio_map, pollfd *client, string &active, string &metadata, int cursor) {
    const string telnet_endl = "\n\u001B[1G";

    // Clears terminal and sets cursor to initial position.
    string msg = "\u001B[2J\n";
    msg += "\u001B[1;1H";
    msg += "Szukaj pośrednika" + telnet_endl;

    for (auto it = radio_map.begin(); it != radio_map.end(); it++) {
        msg += it->second.name;
        if (it->second.address == active)
            msg += " *";
        msg += telnet_endl;
    }

    msg += "Koniec" + telnet_endl;

    if (metadata != "") {
        msg += metadata + telnet_endl;
    }
    msg += "\u001B[" + to_string(cursor) + ";1H";

    if (client[2].fd != -1) {
        tcp_write(client[2].fd, msg);
    }
}

// Checks if any new connections are pending on control port.
// If there are and no control telnet is connected, accepts them.
void manage_control_connections(pollfd *client, bool &telnet_update_needed) {
    if ((client[1].revents & POLLIN)) {
        telnet_update_needed = true;
        int msgsock = accept(client[1].fd, (sockaddr*)0, (socklen_t*)0);

        if (msgsock == -1)
            syserr("accept");
        else {
            if (client[2].fd == -1) {
                client[2].fd = msgsock;
                client[2].events = POLLIN;

                // Set telnet to character mode.
                tcp_write(msgsock, "\377\375\042\377\373\001");
            } else {
                cerr << "A controller telnet is already connected\n";
                close_socket(msgsock);
            }
        }
    }
}

// Checks if a message with audio/metadata or an iam came.
// If so, handles it in a proper way.
void music_socket(pollfd *client, map<string, Radio> &radio_map, string &active_radio_address,
        string &current_metadata, int &cursor, bool &telnet_update_needed) {
    if (client[0].revents & POLLIN) {
        sockaddr_in sender_address;
        string reply;
        uint16_t type;

        ssize_t rcv_len = udp_read(client[0].fd, reply, &sender_address, type);
        string char_address = get_address_string(sender_address);

        if (rcv_len >= 0) {
            if (type == IAM) {
                telnet_update_needed = true;
                int exists = radio_map.count(char_address);

                radio_map[char_address] = Radio(reply, char_address, sender_address);

                if (!exists && distance(radio_map.begin(), radio_map.find(char_address)) <= cursor - 2) {
                    cursor++;
                }
            } else if (type == AUDIO) {
                if (char_address == active_radio_address) {
                    fwrite(reply.c_str(), sizeof(char), rcv_len, stdout);
                }
            } else if (type == METADATA) {
                reply.erase(0, 1);
                current_metadata = reply;
                telnet_update_needed = true;
            } else {
                cerr << "Unknown message type\n";
            }
        } else {
            cerr << "Incorrect UDP header\n";
        }

        if (radio_map.count(char_address)) {
            radio_map[char_address].update_time();
        } else {
            cerr << "Unknown message sender\n";
        }
    }
}

// Checks if any characters came from controlling telnet.
// If so, handles them in a proper way.
void program_control(pollfd *client, map<string, Radio> &radio_map, string &active_radio_address,
        string &current_metadata, int &cursor, timeval &last_keepalive,
        sockaddr_in &multicast_address, bool &telnet_update_needed) {
    if (client[2].fd != -1 && (client[2].revents & (POLLIN | POLLERR))) {
        string read_reply;

        ssize_t rval = tcp_read(client[2].fd, read_reply);

        if (rval <= 0) {
            if (rval < 0) {
                cerr << "Reading message error\n";
            } else {
                cerr << "Ending connection\n";
            }
            close_socket(client[2].fd);
            client[2].fd = -1;
        } else {
            telnet_update_needed = true;
            if (read_reply == "\u001B[B") {
                cursor++;
                cursor = min(cursor, (int)radio_map.size() + 2);
            } else if (read_reply == "\u001B[A") {
                cursor--;
                cursor = max(cursor, 1);
            } else if (read_reply.size() == 2 && read_reply[0] == '\r' && read_reply[1] == '\0') {
                if (cursor == 1) {
                    udp_write(client[0].fd, "", &multicast_address, DISCOVER);
                } else if (cursor == (int)radio_map.size() + 2) {
                    finish_program = true;
                } else {
                    auto it = radio_map.begin();
                    advance(it, cursor - 2);
                    Radio picked_radio = it->second;

                    if (active_radio_address != picked_radio.address)
                        current_metadata = "";

                    active_radio_address = picked_radio.address;
                    last_keepalive = time_now();
                    udp_write(client[0].fd, "", &picked_radio.sock_address, DISCOVER);
                }
            }
        }
    }
}

// Main client functionality.
void run(client_params &params) {
    map<string, Radio> radio_map;
    string active_radio_address = "", current_metadata = "";
    timeval last_keepalive = time_now();
    int cursor = 1;
    sockaddr_in multicast_address;

    pollfd client[3];

    create_poll(client, multicast_address, params.host, params.port, params.control_port);

    // Main program loop
    while (!finish_program) {
        for (int i = 0; i < 3; ++i)
            client[i].revents = 0;

        timeval last_message = time_now();
        if (active_radio_address != "") {
            last_message = radio_map[active_radio_address].last_message;
        }

        // Sets wait time to be at most time left to timeout or next KEEPALIVE.
        long long wait_time = to_usec(time_left(last_message, params.timeout));
        wait_time = min(wait_time, to_usec(time_left(last_keepalive, keepalive_frequency)));

        if (poll(client, 3, wait_time / 1000 + 1) == -1) {
            if (errno != EINTR) {
                syserr("poll");
            } else {
                finish_program = true;
            }
        } else {
            bool telnet_update_needed = false;

            manage_control_connections(client, telnet_update_needed);

            music_socket(client, radio_map, active_radio_address, current_metadata,
                    cursor, telnet_update_needed);

            program_control(client, radio_map, active_radio_address, current_metadata,
                    cursor, last_keepalive, multicast_address, telnet_update_needed);

            if (microseconds_passed_from(last_keepalive) > keepalive_frequency * 1000 && active_radio_address != "") {
                udp_write(client[0].fd, "", &radio_map[active_radio_address].sock_address, KEEPALIVE);
                last_keepalive = time_now();
            }

            if (!check_alive(radio_map, telnet_update_needed, active_radio_address, params.timeout, cursor)) {
                current_metadata = "";
            }
            if (telnet_update_needed) {
                send_update_to_telnet(radio_map, client, active_radio_address, current_metadata, cursor);
            }
        }
    }

    for (int i = 0; i < 3; ++i)
        if (client[i].fd >= 0)
            close_socket(client[i].fd);

}

int main(int argc, char *argv[]) {
    signal(SIGINT, signalHandler);

    client_params params = parse_client_params(argc, argv, print_usage);

    run(params);
}