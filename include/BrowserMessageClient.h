#ifndef BROWSER_MESSAGE_CLIENT_H
#define BROWSER_MESSAGE_CLIENT_H

#include <string>

class BrowserMessageClient {
public:
   void set_server_url(std::string url);
   void send(std::string message);
private:
   std::string server_url;
};

#endif /* BROWSER_MESSAGE_CLIENT_H */