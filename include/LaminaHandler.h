#ifndef LAMINAHANDLER_H
#define LAMINAHANDLER_H

#include <iostream>
#include <string>
#include <iostream>
#include <sstream>
#include <list>
#include <Windows.h>
#include "include/cef_client.h"
#include "include/wrapper/cef_helpers.h"
#include "mruby_lamina.h"
#include "LaminaLifeSpanHandler.h"
#include "LaminaLoadHandler.h"
#include "lamina_opt.h"

using namespace std;

// TODO: Should be "LaminaClient" and all handlers should be extracted
class LaminaHandler : public CefClient,
   public CefDisplayHandler {
public:
   LaminaHandler();
   ~LaminaHandler();

   // Provide access to the single global instance of this object.
   static LaminaHandler* GetInstance();

   // CefClient methods:
   virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE {
      return this;
   }
   virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE {
      return LaminaLifeSpanHandler::GetInstance();
   }
   virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE {
      return LaminaLoadHandler::GetInstance();
   }

   // CefDisplayHandler methods:
   virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) OVERRIDE {
      if (lamina_opt_use_page_titles()) {
         CEF_REQUIRE_UI_THREAD();
         // Set the frame window title bar
         CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
         SetWindowTextW(hwnd, title.c_str());
      }
   }

private:
   // Include the default reference counting implementation.
   IMPLEMENT_REFCOUNTING(LaminaHandler);
};

#endif /*LAMINAHANDLER_H*/
