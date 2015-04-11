#include <thread>
#include "BrowserMessageServer.h"
#include "nn.h"
#include "reqrep.h"
#include "mruby_lamina.h"
#include "LaminaHandler.h"

using namespace std;

void 
BrowserMessageServer::set_url(std::string url) {
   LAMINA_LOG("BrowserMessageServer: Setting browser message server url");
   LAMINA_LOG(url.c_str());
   this->url = url;
   
}

void
BrowserMessageServer::start() {
   LAMINA_LOG("BrowserMessageServer: Starting browser ipc thread");
   thread* browser_ipc_thread = new thread([&](){
      LAMINA_LOG("BrowserMessageServer: Binding socket");
      
      this->socket = nn_socket(AF_SP, NN_REP);
      if ((this->socket) < 0) {
        LAMINA_LOG("BrowserMessageServer: Error creating socket");
      }
      
      if (nn_bind(this->socket, url.c_str()) < 0) {
        LAMINA_LOG("BrowserMessageServer: Error binding socket");
      }

      while (true) {
         void* buf = NULL;
         LAMINA_LOG("BrowserMessageServer: nn_recv");
         int nbytes = nn_recv(this->socket, &buf, NN_MSG, 0);
         LAMINA_LOG("BrowserMessageServer: nn_recv returned");
         if (nbytes > 0) {
            LAMINA_LOG("BrowserMessageServer: Opening new browser window");
            LaminaLifeSpanHandler::GetInstance()->ExecuteJavaScript("window.open('/');", ".*", true);
         }
         nn_freemsg(buf);
      }
   });

   browser_ipc_thread->detach();
}
