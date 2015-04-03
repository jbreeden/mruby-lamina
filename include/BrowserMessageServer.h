#ifndef BROWSER_MESSAGE_SERVER_H
#define BROWSER_MESSAGE_SERVER_H

#include <string>

class BrowserMessageServer {
public:
   void set_url(std::string url);
   void start();

private:
   std::string url;
   int socket;
};

#endif /*BROWSER_MESSAGE_SERVER_H*/