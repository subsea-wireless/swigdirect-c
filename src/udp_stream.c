

#include "udp_stream.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
/* Linux / MacOS POSIX timer headers */
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>



void udp_put_byte(udp_connection_t *udpconn, char data);
void udp_transmit(udp_connection_t *udpconn);
bool udp_buffer_empty(udp_connection_t *udpconn);
int  udp_bytes_available(udp_connection_t *udpconn);
char udp_get_byte(udp_connection_t *udpconn);
void udp_stream_init_write_socket(udp_connection_t *udpconn, const char *target_ip, int port);
void udp_stream_init_read_socket(udp_connection_t *udpconn, int port);
int udp_read2buf(udp_connection_t *udpconn, uint8_t buffer[], int read_length);

void udp_put_byte(udp_connection_t *udpconn, char data) { buffer_put(&udpconn->udp_buffer, data); }

void udp_transmit(udp_connection_t *udpconn)
{
    int i;
    if (buffer_empty(&udpconn->udp_buffer)) return;
    uint8_t buf[BUFFER_SIZE];
    int     no_of_bytes = buffer_bytes_available(&(udpconn->udp_buffer));
    for (i = 0; i < no_of_bytes; i++)
    {
        buf[i] = buffer_get(&udpconn->udp_buffer);
    }

    int bytes_sent = sendto(udpconn->sock, &buf, no_of_bytes, 0, (struct sockaddr *)&(udpconn->Addr), sizeof(struct sockaddr_in));
    if (bytes_sent == no_of_bytes)
    {
        buffer_clear(&udpconn->udp_buffer);
    }
    else
    {  // shuffle unsent bytes back into buffer
        for (i = bytes_sent; i < no_of_bytes; i++)
        {
            buffer_put(&udpconn->udp_buffer, buf[i]);
        }
    }
}

bool udp_buffer_empty(udp_connection_t *udpconn)
{
    // if (buffer_bytes_available(&udpconn->udp_buffer)!=0) {
    //	udp_transmit(udpconn);
    //}
    return (udp_bytes_available(udpconn) == 0);
}

int udp_bytes_available(udp_connection_t *udpconn)
{
    int            i;
    int            recsize;
    char           buf[BUFFER_SIZE];
    socklen_t      fromlen;
    fd_set         rset;
    struct timeval tv;
    FD_ZERO(&rset);
    FD_SET(udpconn->sock, &rset);
    tv.tv_sec  = 0;
    tv.tv_usec = 100;
    if (buffer_bytes_available(&udpconn->udp_buffer) == 0)
    {
        if (select(udpconn->sock + 1, &rset, NULL, NULL, &tv))
        {
            struct sockaddr_in sender;
            fromlen = sizeof(sender);
            recsize = recvfrom(udpconn->sock, (void *)buf, BUFFER_SIZE, 0, (struct sockaddr *)&(sender), &fromlen);

            char ipAddress[20];
            inet_ntop(AF_INET, &(sender.sin_addr), ipAddress, 20);
            // print_util_dbg_printf("rec. UDP from %s:%i, %i\n", ipAddress, ntohs(sender.sin_port), recsize);

            // if (recsize!=-1) printf("rec: %i\n", recsize);
            for (i = 0; i < recsize; i++)
            {
                buffer_put(&udpconn->udp_buffer, buf[i]);
                // print_util_dbg_print(" ");print_util_dbg_print_num((uint8_t)buf[i], 16);
            }  // print_util_dbg_print("--\n");
        }
    }
    return buffer_bytes_available(&udpconn->udp_buffer);
}

char udp_get_byte(udp_connection_t *udpconn) { return buffer_get(&udpconn->udp_buffer); }

void udp_stream_init_write_socket(udp_connection_t *udpconn, const char *target_ip, int port)
{
    // udpconn->sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    memset(&udpconn->Addr, 0, sizeof(udpconn->Addr));
    udpconn->Addr.sin_family      = AF_INET;
    udpconn->Addr.sin_addr.s_addr = inet_addr(target_ip);
    udpconn->Addr.sin_port        = htons(port);
}

void udp_stream_init_read_socket(udp_connection_t *udpconn, int port)
{
    udpconn->sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&udpconn->Addr, 0, sizeof(udpconn->Addr));
    udpconn->Addr.sin_family      = AF_INET;
    udpconn->Addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udpconn->Addr.sin_port        = htons(port);

    // int so_reuseaddr = true;

    // int yes = 1;
    // if ( setsockopt(udpconn->sock, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int)) == -1 )
    //{
    //	perror("setsockopt");
    //}

    if (-1 == bind(udpconn->sock, (struct sockaddr *)&(udpconn->Addr), sizeof(struct sockaddr)))
    {
        printf("error bind failed\n");
        close(udpconn->sock);
        exit(-1);
    }

    /* Attempt to make it non blocking */
    if (fcntl(udpconn->sock, F_SETFL, O_NONBLOCK) < 0)
    {
        fprintf(stderr, "error setting nonblocking: %s\n", strerror(errno));
        close(udpconn->sock);
        exit(-1);
    } /**/
    printf("success binding to port %i: %i\n", port, udpconn->sock);
}

void register_write_stream_udp(byte_stream_t *stream, udp_connection_t *udpconn, const char *target_ip, int port)
{
    udp_stream_init_write_socket(udpconn, target_ip, port);

    buffer_init(&(udpconn->udp_buffer));
    stream->data               = udpconn;
    stream->put                = (int8_t(*)(stream_data_t *, uint8_t)) & udp_put_byte;
    stream->flush              = (void (*)(stream_data_t *)) & udp_transmit;
    stream->start_transmission = (void (*)(stream_data_t *)) & udp_transmit;
    stream->get                = NULL;
    stream->buffer_empty       = (bool (*)(stream_data_t *)) & udp_buffer_empty;
}

void register_read_stream_udp(byte_stream_t *stream, udp_connection_t *udpconn, int port)
{
    udp_stream_init_read_socket(udpconn, port);
    buffer_init(&udpconn->udp_buffer);

    stream->data            = udpconn;
    stream->bytes_available = (int (*)(stream_data_t *)) & udp_bytes_available;
    stream->buffer_empty    = (bool (*)(stream_data_t *)) & udp_buffer_empty;
    stream->get             = (uint8_t(*)(stream_data_t *)) & udp_get_byte;
    stream->read2buf        = (int(*)(stream_data_t *, uint8_t *, int)) & udp_read2buf;
}

int udp_read2buf(udp_connection_t *udpconn, uint8_t buffer[], int read_length) {
    int index = 0;
    while ((index<read_length) && (udp_bytes_available(udpconn)>0)) {
        buffer[index] = udp_get_byte(udpconn);
        index++;
    }
    return index;
}
