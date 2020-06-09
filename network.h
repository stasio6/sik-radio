#ifndef DUZE_NETWORK_H
#define DUZE_NETWORK_H

#include <string>

const uint16_t DISCOVER = 1;
const uint16_t IAM = 2;
const uint16_t KEEPALIVE = 3;
const uint16_t AUDIO = 4;
const uint16_t METADATA = 6;

// Performs a TCP read from socket sock, saving the message to @result.
ssize_t tcp_read(int sock, std::string &result);

// Performs a TCP read to socket sock, sending @message.
void tcp_write(int socket, std::string message);

// Performs a UDP read from socket sock, saving the message to @result.
// If @address is not nullptr, saves the sender address to it.
// Reads using the protocol given in the task statement, saving the type to @type.
ssize_t udp_read(int socket, std::string &result, sockaddr_in *address, uint16_t &type);

// Performs a UDP write to socket sock, reading the message from @message.
// If @address is not nullptr, sends the message to given address, otherwise writes to socket.
// Writes using the protocol given in the task statement, reading the type from @type.
void udp_write(int socket, std::string message, sockaddr_in *address, uint16_t type);

// Performs a select on two sockets, with a given timeout.
// If second socket is a non-positive number, it is ignored in the select.
// Returns number of events, or -1, if select was interrupted.
// Changes each socket to 1, if an event occurred, to -1 otherwise.
int custom_select(int &sock1, int &sock2, timeval timeout);

// Converts sockaddr_in object into a unique string.
std::string get_address_string(sockaddr_in &address);

#endif //DUZE_NETWORK_H
