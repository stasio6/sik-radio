#ifndef DUZE_SOCKET_MANAGER_H
#define DUZE_SOCKET_MANAGER_H

#include <poll.h>

// Creates a socket connected to a given address on a given port.
int create_connected_socket(std::string address, int port);

// Creates a socket connected to a given address on a given port.
// If address is not an empty string, it attaches given in it
// multicast address to the socket.
std::pair<int, ip_mreq> create_multicast_socket(std::string address, int port);

// Sets up pollfd array.
// The first socket is open and can send messages
// to a given (not necessarily multicast) address and port.
// The second socket is listening to incoming connections on a given port.
// The rest of the array is ready to handle new connections.
void create_poll(pollfd *client, sockaddr_in &multicast_address,
                 std::string &host, int port, int control_port);

// Closes a socket with a attached multicast address.
void close_multicast_socket(int sock, ip_mreq ip_mreq);

// Closes a socket.
void close_socket(int sock);

#endif //DUZE_SOCKET_MANAGER_H
