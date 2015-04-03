#include <string>
#include "lamina_util.h"
#include "mruby_lamina.h"

using namespace std;

/* Networking */

apr_socket_t* lamina_util_open_tcp_socket(apr_port_t port, apr_pool_t* pool) {
   apr_socket_t *socket;
   apr_socket_create(&socket, APR_INET, SOCK_STREAM, APR_PROTO_TCP, pool);
   apr_sockaddr_t *addr;
   apr_sockaddr_info_get(&addr, NULL, APR_INET, port, 0, pool);
   apr_socket_bind(socket, addr);
   return socket;
}

int lamina_util_get_open_port(apr_pool_t* pool) {
   apr_socket_t* socket = lamina_util_open_tcp_socket(0, pool);
   apr_sockaddr_t *addr;
   apr_socket_addr_get(&addr, apr_interface_e::APR_LOCAL, socket);
   apr_socket_close(socket);
   return addr->port;
}