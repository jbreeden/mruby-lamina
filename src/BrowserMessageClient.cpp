// #include "BrowserMessageClient.h"
// #include "nn.h"
// #include "reqrep.h"
//
// using namespace std;
//
// void
// BrowserMessageClient::set_server_url(string url) {
//    server_url = url;
// }
//
// void
// BrowserMessageClient::send(string message) {
//    int socket = nn_socket(AF_SP, NN_REQ);
//    int connection = nn_connect(socket, server_url.c_str());
//    int send = nn_send(socket, message.c_str(), message.size() + 1, 0);
// }
