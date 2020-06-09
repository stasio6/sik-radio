#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <unistd.h>

#include "err.h"
#include "network.h"

using namespace std;

namespace {
    const int BUFFER_SIZE = 2000;

    // defined globally, to not allocate it in each call of a function
    char buffer[BUFFER_SIZE + 5];
}

void make_header(uint16_t type, uint16_t length, char *buf) {
    type = htons(type);
    length = htons(length);

    memcpy(buf, &type, 2);
    memcpy(buf + 2, &length, 2);
}

void decode_header(char *buf, uint16_t &type, uint16_t &length) {
    memcpy(&type, buf, 2);
    memcpy(&length, buf + 2, 2);

    type = ntohs(type);
    length = ntohs(length);
}

ssize_t tcp_read(int sock, string &result) {
    int rcv_len = read(sock, buffer, BUFFER_SIZE);
    if (rcv_len > 0) {
        result.assign(buffer, rcv_len);
    }
    return rcv_len;
}

void tcp_write(int socket, string message) {
    if (write(socket, message.c_str(), message.size()) != (ssize_t)message.size()) {
        syserr("partial / failed write");
    }
}

ssize_t udp_single_read(int socket, sockaddr_in *address) {
    if (address) {
        socklen_t salen = sizeof(*address);
        return recvfrom(socket, buffer, BUFFER_SIZE, 0, (sockaddr *) address,
                        &salen);
    } else {
        return read(socket, buffer, BUFFER_SIZE);
    }
}

void udp_single_write(int socket, const char *buf, ssize_t size, sockaddr_in *address) {
    if (address) {
        if (sendto(socket, buf, size, 0, (const sockaddr *) address,
                sizeof *address) != size)
            syserr("write");
    } else {
        if (write(socket, buf, size) != size) {
            syserr("write");
        }
    }
}

ssize_t udp_read(int socket, string &result, sockaddr_in *address, uint16_t &type) {
    ssize_t rcv_len = udp_single_read(socket, address);

    if (rcv_len < 0) {
        syserr("read");
    } else if (rcv_len < 4) {
        return -1;
    }

    uint16_t message_size;
    decode_header(buffer, type, message_size);
    result.assign(buffer + 4, rcv_len - 4);

    while (result.size() < message_size) {
        rcv_len = udp_single_read(socket, address);
        if (rcv_len < 0) {
            syserr("read");
        }
        result += string(buffer, rcv_len);
    }
    return result.size();
}

void udp_write(int socket, string message, sockaddr_in *address, uint16_t type) {
    message = "####" + message;
    int size_left = message.size() - 4;
    int current_position = 4;
    bool need_any_write = true;

    while (size_left > 0 || need_any_write) {
        need_any_write = false;
        int to_send_now = min(size_left, BUFFER_SIZE - 4);

        make_header(type, to_send_now, (char *)message.c_str() + current_position - 4);

        udp_single_write(socket, message.c_str() + current_position - 4, to_send_now + 4, address);

        current_position += to_send_now;
        size_left -= to_send_now;
    }
}

int custom_select(int &sock1, int &sock2, timeval timeout) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock1, &readfds);
    if (sock2 > 0) {
        FD_SET(sock2, &readfds);
    }

    int result;
    if ((result = select(max(sock1, sock2) + 1, &readfds, NULL, NULL, &timeout)) < 0) {
        if (errno != EINTR)
            syserr("select");
        return -1;
    }

    if (FD_ISSET(sock1, &readfds)) {
        sock1 = 1;
    } else {
        sock1 = -1;
    }

    if (sock2 > 0 && FD_ISSET(sock2, &readfds)) {
        sock2 = 1;
    } else {
        sock2 = -1;
    }

    return result;
}

string get_address_string(sockaddr_in &address) {
    string result = string(inet_ntoa(address.sin_addr));
    result += ":" + to_string(ntohs(address.sin_port));
    return result;
}