#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <string>
#include <tuple>
#include <unistd.h>

#include "err.h"
#include "socket_manager.h"

using namespace std;

int create_connected_socket(string address, int port) {
    int sock;
    addrinfo addr_hints;
    addrinfo *addr_result;

    int err;

    // 'converting' host/port in string to addrinfo
    memset(&addr_hints, 0, sizeof(addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    err = getaddrinfo(address.c_str(), to_string(port).c_str(), &addr_hints, &addr_result);
    if (err == EAI_SYSTEM) { // system error
        syserr("getaddrinfo: %s", gai_strerror(err));
    }
    else if (err != 0) { // other error (host not found, etc.)
        fatal("getaddrinfo: %s", gai_strerror(err));
    }

    // initialize socket according to getaddrinfo results
    sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (sock < 0)
        syserr("socket");

    // connect socket to the server
    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
        syserr("connect");

    freeaddrinfo(addr_result);

    return sock;
}

pair<int, ip_mreq> create_multicast_socket(string address, int port) {
    /* argumenty wywołania programu */
    in_port_t local_port;

    /* zmienne i struktury opisujące gniazda */
    int sock;
    sockaddr_in local_address;
    ip_mreq ip_mreq;

    local_port = (in_port_t)port;

    /* otwarcie gniazda */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    /* podłączenie do grupy rozsyłania (ang. multicast) */
    if (address != "") {
        char *multicast_dotted_address = (char *) address.c_str();
        ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0) {
            fatal("inet_aton - invalid multicast address");
        }
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &ip_mreq, sizeof ip_mreq) < 0)
            syserr("setsockopt");
    }

    /* ustawienie adresu i portu lokalnego */
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(local_port);
    if (::bind(sock, (sockaddr *)&local_address, sizeof local_address) < 0)
        syserr("bind");

    return make_pair(sock, ip_mreq);
}

pair<int, sockaddr_in> poll_multicast_socket(string host, int port) {
    /* argumenty wywołania programu */
    char *remote_dotted_address;
    in_port_t remote_port;

    /* zmienne i struktury opisujące gniazda */
    int sock, optval;
    sockaddr_in remote_address;

    /* zmienne obsługujące komunikację */

    remote_dotted_address = (char *)host.c_str();
    remote_port = (in_port_t)port;

    /* otwarcie gniazda */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    /* ustawienie TTL dla datagramów rozsyłanych do grupy */
    optval = 4;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt multicast ttl");

    optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof (optval)) < 0)
        syserr("setsockopt broadcast");

    /* ustawienie adresu i portu odbiorcy */
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(remote_port);
    if (inet_aton(remote_dotted_address, &remote_address.sin_addr) == 0) {
        fatal("inet_aton - invalid multicast address");
    }
    return make_pair(sock, remote_address);
}

void create_poll(pollfd *client, sockaddr_in &multicast_address,
        string &host, int port, int control_port) {

    /* Inicjujemy tablicę z gniazdkami klientów, client[0] to gniazdko centrali */
    for (int i = 0; i < 3; ++i) {
        client[i].fd = -1;
        client[i].events = POLLIN;
        client[i].revents = 0;
    }

    /* Tworzymy gniazdko centrali */
    tie(client[0].fd, multicast_address) = poll_multicast_socket(host, port);
    if (client[0].fd == -1)
        syserr("Opening stream socket");

    /* Tworzymy gniazdko kontrolne */
    client[1].fd = socket(PF_INET, SOCK_STREAM, 0);
    if (client[1].fd == -1)
        syserr("Opening stream socket");

    /* A teraz serwer kontrolny */
    sockaddr_in control_server;
    control_server.sin_family = AF_INET;
    control_server.sin_addr.s_addr = htonl(INADDR_ANY);
    control_server.sin_port = htons(control_port);
    if (::bind(client[1].fd, (sockaddr *) &control_server,
        (socklen_t) sizeof(control_server)) == -1)
            syserr("Binding stream socket");

    if (listen(client[1].fd, 5) == -1)
        syserr("Starting to listen");
}

void close_multicast_socket(int sock, ip_mreq ip_mreq) {
    /* odłączenie od grupy rozsyłania */
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt");

    /* koniec */
    close_socket(sock);
}

void close_socket(int sock) {
    if (close(sock) < 0)
        syserr("close");
}