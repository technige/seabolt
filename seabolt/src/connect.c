/*
 * Copyright (c) 2002-2017 "Neo Technology,"
 * Network Engine for Objects in Lund AB [http://neotechnology.com]
 *
 * This file is part of Neo4j.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <connect.h>
#include "protocol/v1.h"
#include "buffer.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <connect.h>
#include <values.h>
#include <stdbool.h>
#include "mem.h"


#define INITIAL_TX_BUFFER_SIZE 8192
#define INITIAL_RX_BUFFER_SIZE 8192

#define char_to_uint16be(array) ((uint8_t)(header[0]) << 8) | (uint8_t)(header[1]);

#define SOCKET(domain, type, protocol) socket(domain, type, protocol)
#define CONNECT(socket, address, address_size) connect(socket, address, address_size)
#define SHUTDOWN(socket, how) shutdown(socket, how)
#define TRANSMIT(socket, data, size, flags) (int)(send(socket, data, (size_t)(size), flags))
#define TRANSMIT_S(socket, data, size, flags) SSL_write(socket, data, size)
#define RECEIVE(socket, buffer, size, flags) (int)(recv(socket, buffer, (size_t)(size), flags))
#define RECEIVE_S(socket, buffer, size, flags) SSL_read(socket, buffer, size)


void _copy_ipv6_socket_address(char* target, struct sockaddr_in6* source)
{
    memcpy(&target[0], &source->sin6_addr.__in6_u.__u6_addr8, 16);
}

void _copy_ipv4_socket_address(char* target, struct sockaddr_in* source)
{
    memcpy(&target[0], "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF", 12);
    memcpy(&target[12], &source->sin_addr.s_addr, 4);
}

struct BoltConnection* _create(enum BoltTransport transport)
{
    struct BoltConnection* connection = BoltMem_allocate(sizeof(struct BoltConnection));

    connection->transport = transport;

    connection->socket = 0;
    connection->ssl_context = NULL;
    connection->ssl = NULL;

    connection->protocol_version = 0;
    connection->protocol_state = NULL;

    connection->tx_buffer = BoltBuffer_create(INITIAL_TX_BUFFER_SIZE);
    connection->rx_buffer = BoltBuffer_create(INITIAL_RX_BUFFER_SIZE);

    connection->status = BOLT_DISCONNECTED;
    connection->error = BOLT_NO_ERROR;

    return connection;
}

void _set_status(struct BoltConnection* connection, enum BoltConnectionStatus status, enum BoltConnectionError error)
{
    connection->status = status;
    connection->error = error;
    switch (connection->status)
    {
        case BOLT_DISCONNECTED:
            BoltLog_info("bolt: Disconnected");
            break;
        case BOLT_CONNECTED:
            BoltLog_info("bolt: Connected");
            break;
        case BOLT_READY:
            BoltLog_info("bolt: Ready");
            break;
        case BOLT_FAILED:
            BoltLog_info("bolt: FAILED");
            break;
        case BOLT_DEFUNCT:
            BoltLog_info("bolt: DEFUNCT");
            break;
    }
}

int _open_b(struct BoltConnection* connection, struct sockaddr* address, socklen_t address_size)
{
    switch (address->sa_family)
    {
        case AF_INET:
        {
            char address_string[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)(address))->sin_addr,
                      &address_string[0], sizeof(address_string));
            BoltLog_info("bolt: Opening IPv4 connection to %s", &address_string);
            break;
        }
        case AF_INET6:
        {
            char address_string[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)(address))->sin6_addr,
                      &address_string[0], sizeof(address_string));
            BoltLog_info("bolt: Opening IPv6 connection to %s", &address_string);
            break;
        }
        default:
            BoltLog_error("bolt: Unsupported address family %d", address->sa_family);
            _set_status(connection, BOLT_DEFUNCT, BOLT_UNSUPPORTED);
            return -1;
    }
    connection->socket = SOCKET(address->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (connection->socket == -1)
    {
        // TODO: Windows
        switch(errno)
        {
            case EACCES:
                _set_status(connection, BOLT_DEFUNCT, BOLT_PERMISSION_DENIED);
                return -1;
            case EAFNOSUPPORT:
            case EINVAL:
            case EPROTONOSUPPORT:
                _set_status(connection, BOLT_DEFUNCT, BOLT_UNSUPPORTED);
                return -1;
            case EMFILE:
            case ENFILE:
                _set_status(connection, BOLT_DEFUNCT, BOLT_OUT_OF_FILES);
                return -1;
            case ENOBUFS:
            case ENOMEM:
                _set_status(connection, BOLT_DEFUNCT, BOLT_OUT_OF_MEMORY);
                return -1;
            default:
                _set_status(connection, BOLT_DEFUNCT, BOLT_UNKNOWN_ERROR);
                return -1;
        }
    }
    int connected = CONNECT(connection->socket, address, address_size);
    if(connected == -1)
    {
        // TODO: Windows
        switch(errno)
        {
            case EACCES:
            case EPERM:
                _set_status(connection, BOLT_DEFUNCT, BOLT_PERMISSION_DENIED);
                return -1;
            case EAFNOSUPPORT:
                _set_status(connection, BOLT_DEFUNCT, BOLT_UNSUPPORTED);
                return -1;
            case EAGAIN:
                _set_status(connection, BOLT_DEFUNCT, BOLT_OUT_OF_PORTS);
                return -1;
            case ECONNREFUSED:
                _set_status(connection, BOLT_DEFUNCT, BOLT_CONNECTION_REFUSED);
                return -1;
            case EINTR:
                _set_status(connection, BOLT_DEFUNCT, BOLT_INTERRUPTED);
                return -1;
            case ENETUNREACH:
                _set_status(connection, BOLT_DEFUNCT, BOLT_NETWORK_UNREACHABLE);
                return -1;
            case ETIMEDOUT:
                _set_status(connection, BOLT_DEFUNCT, BOLT_TIMED_OUT);
                return -1;
            default:
                _set_status(connection, BOLT_DEFUNCT, BOLT_UNKNOWN_ERROR);
                return -1;
        }
    }
    _set_status(connection, BOLT_CONNECTED, BOLT_NO_ERROR);
    return 0;
}

int _secure_b(struct BoltConnection* connection)
{
    // TODO: investigate ways to provide a greater resolution of TLS errors
    BoltLog_info("bolt: Securing socket");
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    connection->ssl_context = SSL_CTX_new(TLSv1_2_client_method());
    if (connection->ssl_context == NULL)
    {
        _set_status(connection, BOLT_DEFUNCT, BOLT_TLS_ERROR);
        return -1;
    }
    if (SSL_CTX_set_default_verify_paths(connection->ssl_context) != 1)
    {
        _set_status(connection, BOLT_DEFUNCT, BOLT_TLS_ERROR);
        return -1;
    }
    connection->ssl = SSL_new(connection->ssl_context);
    int linked_socket = SSL_set_fd(connection->ssl, connection->socket);
    if (linked_socket != 1)
    {
        _set_status(connection, BOLT_DEFUNCT, BOLT_TLS_ERROR);
        return -1;
    }
    int connected = SSL_connect(connection->ssl);
    if (connected != 1)
    {
        _set_status(connection, BOLT_DEFUNCT, BOLT_TLS_ERROR);
        return -1;
    }
    return 0;
}

void _close_b(struct BoltConnection* connection)
{
    BoltLog_info("bolt: Closing connection");
    switch(connection->transport)
    {
        case BOLT_INSECURE_SOCKET:
        {
            SHUTDOWN(connection->socket, 2);
            break;
        }
        case BOLT_SECURE_SOCKET:
        {
            if (connection->ssl != NULL)   {
                SSL_shutdown(connection->ssl);
                SSL_free(connection->ssl);
                connection->ssl = NULL;
            }
            if (connection->ssl_context != NULL)   {
                SSL_CTX_free(connection->ssl_context);
                connection->ssl_context = NULL;
            }
            SHUTDOWN(connection->socket, 2);
            break;
        }
    }
    _set_status(connection, BOLT_DISCONNECTED, BOLT_NO_ERROR);
}

void _destroy(struct BoltConnection* connection)
{
    switch(connection->protocol_version)
    {
        case 1:
            BoltProtocolV1_destroy_state(connection->protocol_state);
            break;
        default:
            break;
    }
    BoltBuffer_destroy(connection->rx_buffer);
    BoltBuffer_destroy(connection->tx_buffer);
    BoltMem_deallocate(connection, sizeof(struct BoltConnection));
}

int _transmit_b(struct BoltConnection* connection, const char* data, int size)
{
    if (size == 0)
    {
        return 0;
    }
    int remaining = size;
    int total_sent = 0;
    while (total_sent < size)
    {
        int sent = 0;
        switch(connection->transport)
        {
            case BOLT_INSECURE_SOCKET:
            {
                sent = TRANSMIT(connection->socket, data, size, 0);
                break;
            }
            case BOLT_SECURE_SOCKET:
            {
                sent = TRANSMIT_S(connection->ssl, data, size, 0);
                break;
            }
        }
        if (sent >= 0)
        {
            total_sent += sent;
            remaining -= sent;
        }
        else
        {
            switch (connection->transport)
            {
                case BOLT_INSECURE_SOCKET:
                    _set_status(connection, BOLT_DEFUNCT, errno);
                    BoltLog_error("bolt: Socket error %d on transmit", connection->error);
                    break;
                case BOLT_SECURE_SOCKET:
                    _set_status(connection, BOLT_DEFUNCT, SSL_get_error(connection->ssl, sent));
                    BoltLog_error("bolt: SSL error %d on transmit", connection->error);
                    break;
            }
            return -1;
        }
    }
    BoltLog_info("bolt: Sent %d of %d bytes", total_sent, size);
    return total_sent;
}

/**
 * Attempt to receive between min_size and max_size bytes.
 *
 * @param connection
 * @param buffer
 * @param min_size
 * @param max_size
 * @return
 */
int _receive_b(struct BoltConnection* connection, char* buffer, int min_size, int max_size)
{
    if (min_size == 0)
    {
        return 0;
    }
    int max_remaining = max_size;
    int total_received = 0;
    while (total_received < min_size)
    {
        int received = 0;
        switch (connection->transport)
        {
            case BOLT_INSECURE_SOCKET:
                received = RECEIVE(connection->socket, buffer, max_remaining, 0);
                break;
            case BOLT_SECURE_SOCKET:
                received = RECEIVE_S(connection->ssl, buffer, max_remaining, 0);
                break;
        }
        if (received > 0)
        {
            total_received += received;
            max_remaining -= received;
        }
        else if (received == 0)
        {
            BoltLog_info("bolt: Detected end of transmission");
            _set_status(connection, BOLT_DISCONNECTED, BOLT_END_OF_TRANSMISSION);
            break;
        }
        else
        {
            switch (connection->transport)
            {
                case BOLT_INSECURE_SOCKET:
                    _set_status(connection, BOLT_DEFUNCT, errno);
                    BoltLog_error("bolt: Socket error %d on receive", connection->error);
                    break;
                case BOLT_SECURE_SOCKET:
                    _set_status(connection, BOLT_DEFUNCT, SSL_get_error(connection->ssl, received));
                    BoltLog_error("bolt: SSL error %d on receive", connection->error);
                    break;
            }
            return -1;
        }
    }
    BoltLog_info("bolt: Received %d of %d..%d bytes", total_received, min_size, max_size);
    return total_received;
}

int _fetch_b(struct BoltConnection * connection, char * buffer, int size)
{
    if (size == 0) return 0;
    int available = BoltBuffer_unloadable(connection->rx_buffer);
    if (size > available)
    {
        int delta = size - available;
        while (delta > 0)
        {
            int max_size = BoltBuffer_loadable(connection->rx_buffer);
            if (max_size == 0)
            {
                BoltBuffer_compact(connection->rx_buffer);
                max_size = BoltBuffer_loadable(connection->rx_buffer);
            }
            max_size = delta > max_size ? delta : max_size;
            int received = _receive_b(connection, BoltBuffer_load_target(connection->rx_buffer, max_size), delta, max_size);
            if (received == -1)
            {
                return -1;
            }
            // adjust the buffer extent based on the actual amount of data received
            connection->rx_buffer->extent = connection->rx_buffer->extent - max_size + received;
            delta -= received;
        }
    }
    BoltBuffer_unload(connection->rx_buffer, buffer, size);
    return size;
}

int _handshake_b(struct BoltConnection* connection, int32_t _1, int32_t _2, int32_t _3, int32_t _4)
{
    BoltLog_info("bolt: Performing handshake");
    char handshake[20];
    memcpy(&handshake[0x00], "\x60\x60\xB0\x17", 4);
    memcpy_be(&handshake[0x04], &_1, 4);
    memcpy_be(&handshake[0x08], &_2, 4);
    memcpy_be(&handshake[0x0C], &_3, 4);
    memcpy_be(&handshake[0x10], &_4, 4);
    try(_transmit_b(connection, &handshake[0], 20));
    try(_receive_b(connection, &handshake[0], 4, 4));
    memcpy_be(&connection->protocol_version, &handshake[0], 4);
    BoltLog_info("bolt: Using Bolt v%d", connection->protocol_version);
    switch(connection->protocol_version)
    {
        case 1:
            connection->protocol_state = BoltProtocolV1_create_state();
            return 0;
        default:
            _close_b(connection);
            _set_status(connection, BOLT_DEFUNCT, BOLT_UNSUPPORTED);
            return -1;
    }
}

struct BoltAddress* BoltAddress_create(const char * host, const char * port)
{
    struct BoltAddress* service = BoltMem_allocate(sizeof(struct BoltAddress));
    service->host = host;
    service->port = port;
    service->n_resolved_hosts = 0;
    service->resolved_hosts = BoltMem_allocate(0);
    service->resolved_port = 0;
    service->gai_status = 0;
    return service;
}

void BoltAddress_resolve_b(struct BoltAddress * address)
{
    static struct addrinfo hints;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = (AI_V4MAPPED | AI_ADDRCONFIG);
    struct addrinfo* ai;
    address->gai_status = getaddrinfo(address->host, NULL, &hints, &ai);
    BoltLog_info("bolt: gai status = %d", address->gai_status);
    if (address->gai_status == 0)
    {
        unsigned short n_resolved = 0;
        for (struct addrinfo* ai_node = ai; ai_node != NULL; ai_node = ai_node->ai_next)
        {
            switch (ai_node->ai_family)
            {
                case AF_INET:
                case AF_INET6:
                {
                    n_resolved += 1;
                    break;
                }
                default:
                    // We only care about IPv4 and IPv6
                    continue;
            }
        }
        address->resolved_hosts = BoltMem_reallocate(address->resolved_hosts,
                                                     address->n_resolved_hosts * 16U, n_resolved * 16U);
        address->n_resolved_hosts = n_resolved;
        size_t p = 0;
        for (struct addrinfo* ai_node = ai; ai_node != NULL; ai_node = ai_node->ai_next)
        {
            switch (ai_node->ai_family)
            {
                case AF_INET:
                {
                    _copy_ipv4_socket_address(BoltAddress_resolved_host(address, p), (struct sockaddr_in*)(ai_node->ai_addr));
                    break;
                }
                case AF_INET6:
                {
                    _copy_ipv6_socket_address(BoltAddress_resolved_host(address, p), (struct sockaddr_in6*)(ai_node->ai_addr));
                    break;
                }
                default:
                    // We only care about IPv4 and IPv6
                    continue;
            }
            p += 1;
        }
        freeaddrinfo(ai);
    }

    char *c;
    address->resolved_port = (in_port_t)(strtoul(address->port, &c, 10));
    if (*c != '\0')
    {
        struct servent * entry = getservbyname(address->port, "TCP");
        address->resolved_port = (in_port_t)(entry->s_port);
    }
}

char * BoltAddress_resolved_host(struct BoltAddress * address, size_t index)
{
    return &address->resolved_hosts[16U * index];
}

int BoltAddress_resolved_host_is_ipv4(struct BoltAddress * address, size_t index)
{
    const char * resolved_host = BoltAddress_resolved_host(address, index);
    for (int i = 0; i < 10; i++)
    {
        if (resolved_host[i] != 0) return 0;
    }
    for (int i = 10; i < 12; i++)
    {
        if (resolved_host[i] != -1) return 0;
    }
    return 1;
}

void BoltAddress_destroy(struct BoltAddress * address)
{
    BoltMem_deallocate(address->resolved_hosts, address->n_resolved_hosts * 16U);
    BoltMem_deallocate(address, sizeof(struct BoltAddress));
}

void BoltAddress_write(struct BoltAddress * address, FILE * file)
{
    fprintf(file, "BoltAddress(host=\"%s\" port=\"%s\" resolved_hosts=IPv6[", address->host, address->port);
    for(size_t i = 0; i < address->n_resolved_hosts; i++)
    {
        if (i > 0) fprintf(file, ", ");
        char out[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, BoltAddress_resolved_host(address, i), &out[0], INET6_ADDRSTRLEN);
        fprintf(file, "\"%s\"", out);
    }
    fprintf(file, "] resolved_port=%d)", address->resolved_port);
}

struct BoltConnection* BoltConnection_open_b(enum BoltTransport transport, struct BoltAddress* address)
{
    struct BoltConnection* connection = _create(transport);
    if (address->n_resolved_hosts > 0)
    {
        for (size_t i = 0; i < address->n_resolved_hosts; i++)
        {
            int opened;
            if (BoltAddress_resolved_host_is_ipv4(address, i))
            {
                struct sockaddr_in sa;
                sa.sin_family = AF_INET;
                sa.sin_port = htons(address->resolved_port);
                memcpy(&sa.sin_addr.s_addr, BoltAddress_resolved_host(address, i) + 12, 4);
                opened = _open_b(connection, (struct sockaddr *)(&sa), sizeof(sa));
            }
            else
            {
                struct sockaddr_in6 sa;
                sa.sin6_family = AF_INET6;
                sa.sin6_port = htons(address->resolved_port);
                memcpy(&sa.sin6_addr.__in6_u.__u6_addr8, BoltAddress_resolved_host(address, i), 16);
                opened = _open_b(connection, (struct sockaddr *)(&sa), sizeof(sa));
            }
            if (opened == 0)
            {
                if (transport == BOLT_SECURE_SOCKET)
                {
                    int secured = _secure_b(connection);
                    if (secured == 0)
                    {
                        _handshake_b(connection, 1, 0, 0, 0);
                    }
                }
                else
                {
                    _handshake_b(connection, 1, 0, 0, 0);
                }
                break;
            }
        }
    }
    else
    {
        _set_status(connection, BOLT_DEFUNCT, BOLT_UNRESOLVED_ADDRESS);
    }
    return connection;
}

void BoltConnection_close_b(struct BoltConnection* connection)
{
    if (connection->status != BOLT_DISCONNECTED)
    {
        _close_b(connection);
    }
    _destroy(connection);
}

int BoltConnection_send_b(struct BoltConnection * connection)
{
    int size = BoltBuffer_unloadable(connection->tx_buffer);
    int transmitted = _transmit_b(connection, BoltBuffer_unload_target(connection->tx_buffer, size), size);
    if (transmitted == -1)
    {
        return -1;
    }
    BoltBuffer_compact(connection->tx_buffer);
    struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
    if (state == NULL)
    {
        return 0;
    }
    BoltLog_info("bolt: Sent up to request #%d", state->next_request_id - 1);
    return state->next_request_id - 1;
}

int BoltConnection_fetch_b(struct BoltConnection * connection, int request_id)
{
    switch (connection->protocol_version)
    {
        case 1:
        {
            int records = 0;
            struct BoltProtocolV1State * state = BoltProtocolV1_state(connection);
            int response_id;
            do
            {
                char header[2];
                int fetched = _fetch_b(connection, &header[0], 2);
                if (fetched == -1)
                {
                    BoltLog_error("Could not fetch chunk header");
                    return -1;
                }
                uint16_t chunk_size = char_to_uint16be(header);
                BoltBuffer_compact(state->rx_buffer);
                while (chunk_size != 0)
                {
                    fetched = _fetch_b(connection, BoltBuffer_load_target(state->rx_buffer, chunk_size), chunk_size);
                    if (fetched == -1)
                    {
                        BoltLog_error("Could not fetch chunk data");
                        return -1;
                    }
                    fetched = _fetch_b(connection, &header[0], 2);
                    if (fetched == -1)
                    {
                        BoltLog_error("Could not fetch chunk header");
                        return -1;
                    }
                    chunk_size = char_to_uint16be(header);
                }
                response_id = state->response_counter;
                BoltProtocolV1_unload(connection);
                if (BoltValue_type(state->fetched) == BOLT_SUMMARY)
                {
                    state->response_counter += 1;
                }
                else
                {
                    records += 1;
                }
            } while (response_id != request_id);
            if (BoltValue_type(state->fetched) == BOLT_SUMMARY)
            {
                int16_t code = BoltSummary_code(state->fetched);
                switch (code)
                {
                    case 0x70:  // SUCCESS
                        BoltLog_info("bolt: Request #%d succeeded", response_id);
                        _set_status(connection, BOLT_READY, BOLT_NO_ERROR);
                        return records;
                    case 0x7E:  // IGNORED
                        BoltLog_info("bolt: Request #%d ignored", response_id);
                        return records;
                    case 0x7F:  // FAILURE
                        BoltLog_error("bolt: Request %d failed", response_id);
                        _set_status(connection, BOLT_FAILED, BOLT_UNKNOWN_ERROR);   // TODO more specific error
                        return -1;
                    default:
                        BoltLog_error("bolt: Protocol violation (received summary code %d)", code);
                        _set_status(connection, BOLT_DEFUNCT, BOLT_PROTOCOL_VIOLATION);
                        return -1;
                }
            }
            return records;
        }
        default:
        {
            // TODO
            return -1;
        }
    }
}

int BoltConnection_fetch_summary_b(struct BoltConnection * connection, int request_id)
{
    int records = 0;
    int more = 1;
    while (more)
    {
        int new_records = BoltConnection_fetch_b(connection, request_id);
        if (new_records == -1)
        {
            return -1;
        }
        records += new_records;
        more = new_records > 0;
    }
    return records;
}

struct BoltValue* BoltConnection_fetched(struct BoltConnection * connection)
{
    switch (connection->protocol_version)
    {
        case 1:
        {
            struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
            return state->fetched;
        }
        default:
            return NULL;
    }
}

int BoltConnection_init_b(struct BoltConnection* connection, const char* user_agent, const char* user, const char* password)
{
    BoltLog_info("bolt: Initialising connection for user '%s'", user);
    switch (connection->protocol_version)
    {
        case 1:
        {
            struct BoltValue* init = BoltValue_create();
            BoltProtocolV1_compile_INIT(init, user_agent, user, password);
            int init_id = BoltProtocolV1_load(connection, init);
            BoltValue_destroy(init);
            try(BoltConnection_send_b(connection));
            BoltConnection_fetch_summary_b(connection, init_id);
            struct BoltValue* last_received = BoltConnection_fetched(connection);
            int16_t code = BoltSummary_code(last_received);
            switch (code)
            {
                case 0x70:  // SUCCESS
                    BoltLog_info("bolt: Initialisation SUCCESS");
                    _set_status(connection, BOLT_READY, BOLT_NO_ERROR);
                    return 0;
                case 0x7F:  // FAILURE
                    BoltLog_error("bolt: Initialisation FAILURE");
                    _set_status(connection, BOLT_DEFUNCT, BOLT_PERMISSION_DENIED);
                    return -1;
                default:
                    BoltLog_error("bolt: Protocol violation (received summary code %d)", code);
                    _set_status(connection, BOLT_DEFUNCT, BOLT_PROTOCOL_VIOLATION);
                    return -1;
            }
        }
        default:
            _set_status(connection, BOLT_DEFUNCT, BOLT_UNSUPPORTED);
            return -1;
    }
}

int BoltConnection_set_cypher_template(struct BoltConnection * connection, const char * statement, size_t size)
{
    if (size <= INT32_MAX)
    {
        struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
        BoltValue_to_String8(state->run.statement, statement, (int32_t)(size));
        return 0;
    }
    return -1;
}

int BoltConnection_set_n_cypher_parameters(struct BoltConnection * connection, int32_t size)
{
    struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
    BoltValue_to_Dictionary8(state->run.parameters, size);
    return 0;
}

int BoltConnection_set_cypher_parameter_key(struct BoltConnection * connection, int32_t index, const char * key, size_t key_size)
{
    struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
    return BoltDictionary8_set_key(state->run.parameters, index, key, key_size);
}

struct BoltValue* BoltConnection_cypher_parameter_value(struct BoltConnection * connection, int32_t index)
{
    struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
    return BoltDictionary8_value(state->run.parameters, index);
}

int BoltConnection_load_begin_request(struct BoltConnection * connection)
{
    switch (connection->protocol_version)
    {
        case 1:
        {
            struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
            BoltProtocolV1_load(connection, state->begin.request);
            return BoltProtocolV1_load(connection, state->discard_request);
        }
        default:
            return -1;
    }
}

int BoltConnection_load_commit_request(struct BoltConnection * connection)
{
    switch (connection->protocol_version)
    {
        case 1:
        {
            struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
            BoltProtocolV1_load(connection, state->commit.request);
            return BoltProtocolV1_load(connection, state->discard_request);
        }
        default:
            return -1;
    }
}

int BoltConnection_load_rollback_request(struct BoltConnection * connection)
{
    switch (connection->protocol_version)
    {
        case 1:
        {
            struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
            BoltProtocolV1_load(connection, state->rollback.request);
            return BoltProtocolV1_load(connection, state->discard_request);
        }
        default:
            return -1;
    }
}

int BoltConnection_load_run_request(struct BoltConnection * connection)
{
    switch (connection->protocol_version)
    {
        case 1:
        {
            struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
            return BoltProtocolV1_load(connection, state->run.request);
        }
        default:
            return -1;
    }
}

int BoltConnection_load_discard_request(struct BoltConnection * connection, int32_t n)
{
    switch (connection->protocol_version)
    {
        case 1:
            if (n >= 0)
            {
                return -1;
            }
            else
            {
                struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
                return BoltProtocolV1_load(connection, state->discard_request);
            }
        default:
            return -1;
    }
}

int BoltConnection_load_pull_request(struct BoltConnection * connection, int32_t n)
{
    switch (connection->protocol_version)
    {
        case 1:
            if (n >= 0)
            {
                return -1;
            }
            else
            {
                struct BoltProtocolV1State* state = BoltProtocolV1_state(connection);
                return BoltProtocolV1_load(connection, state->pull_request);
            }
        default:
            return -1;
    }
}
