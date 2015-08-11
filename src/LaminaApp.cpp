#include "LaminaHandler.h"
#include <cstdio>
#include <cstring>
#include "LaminaRenderProcessHandler.h"
#include "LaminaApp.h"
#include "lamina_opt.h"

#ifdef _WIN32
  #include <direct.h>
  #define getcwd _getcwd
#else
  #include <unistd.h>
#endif

LaminaApp::LaminaApp() {
}

static CefRefPtr<CefRenderProcessHandler> renderProcessHandler = CefRefPtr<CefRenderProcessHandler>(new LaminaRenderProcessHandler);

CefRefPtr<CefRenderProcessHandler> LaminaApp::GetRenderProcessHandler(){
   return renderProcessHandler;
}

void LaminaApp::OnContextInitialized() {
  cout << "Browser Context Created!" << std::endl;

   // Information used when creating the native window.
   CefWindowInfo window_info;

#if defined(OS_WIN)
   // On Windows we need to specify certain flags that will be passed to
   // CreateWindowEx().
   window_info.SetAsPopup(NULL, lamina_opt_window_title());
#endif

   // LaminaHandler implements browser-level callbacks.
   CefRefPtr<LaminaHandler> handler(new LaminaHandler());

   // Specify CEF browser settings here.
   CefBrowserSettings browser_settings;


   char* url;
   if (g_command_line->HasSwitch("url")) {
     url = (char*)(g_command_line->GetSwitchValue("url").ToString().c_str());
   } else {
     char* cwd = (char*)malloc(1024);
     getcwd(cwd, 1024);
     url = (char*)malloc(1050);
     sprintf(url, "file://");
     strcat(url, cwd);
     FILE* index_file = fopen("index.html", "r");
     if (index_file != NULL) {
       strcat(url, "/index.html");
       fclose(index_file);
     }
   }

   // Create the first browser window.
   cout << "Opening URL: " << url << std::endl;
   CefRefPtr<CefBrowser> browser = CefBrowserHost::CreateBrowserSync(window_info, handler.get(), url, browser_settings, NULL);
   if (g_command_line->HasSwitch("dev-tools")) {
     browser->GetHost()->ShowDevTools(window_info, handler.get(), browser_settings, CefPoint(0, 0));
   }
}
