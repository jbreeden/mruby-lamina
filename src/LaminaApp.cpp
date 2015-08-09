#include "LaminaHandler.h"
#include "LaminaRenderProcessHandler.h"
#include "LaminaApp.h"
#include "lamina_opt.h"

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

   // Create the first browser window.
   cout << "Opening URL: " << this->url << std::endl;
   CefBrowserHost::CreateBrowser(window_info, handler.get(), this->url, browser_settings, NULL);
}
