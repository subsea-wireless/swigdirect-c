#ifndef UDP_STREAM_H
#define UDP_STREAM_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "buffer.h"
#include "streams.h"

#define UDP_BUS_MAX_CLIENTS 10
#define UDP_BUFFER_SIZE     512

typedef struct udp_connection_t
{
    buffer_t           udp_buffer;
    struct sockaddr_in Addr;
    int                sock;
} udp_connection_t;

typedef struct udp_bus_master_t
{
    struct sockaddr_in clients[UDP_BUS_MAX_CLIENTS];
    udp_connection_t   tx;
    udp_connection_t   master;
    int                registered_clients;
    bool               rebroadcast;
} udp_bus_master_t;

typedef struct udp_bus_client_t
{
    udp_connection_t tx;
    udp_connection_t master;
} udp_bus_client_t;

void register_write_stream_udp(byte_stream_t *stream, udp_connection_t *udpconn, const char *target_ip, int port);

void register_read_stream_udp(byte_stream_t *stream, udp_connection_t *udpconn, int port);

void udp_bus_master_init(udp_bus_master_t *data, int port, bool rebroadcast);

void udp_bus_client_init(udp_bus_client_t *data, const char *target_ip, int port);

void register_write_stream_udp_bus(byte_stream_t *stream, udp_bus_master_t *data);

void register_read_stream_udp_bus(byte_stream_t *stream, udp_bus_master_t *data);

int udp_bus_master_register_client(udp_bus_master_t *data, struct sockaddr_in client_addr);

#endif