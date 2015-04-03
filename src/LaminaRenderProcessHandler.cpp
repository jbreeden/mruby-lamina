#define _CRT_SECURE_NO_DEPRECATE

#include "stdio.h"
#include "errno.h"
#include "mruby.h"
#include "mruby_lamina.h"
#include "mruby/compile.h"
#include "mruby_cef.h"
#include "ruby_fn_handler.h"
#include "lamina_opt.h"
#include "LaminaRenderProcessHandler.h"

LaminaRenderProcessHandler::LaminaRenderProcessHandler() {}

void LaminaRenderProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
   LAMINA_LOG("Enter Context Created");
   auto mrb = mrb_for_thread();
   FILE* extensions_script = fopen(lamina_opt_script_v8_extensions().c_str(), "r");
   if (extensions_script != NULL) {
      mrb_load_file(mrb, extensions_script);
   }
   else {
      if (errno != ENOENT) {
         CefRefPtr<CefV8Value> ret;
         CefRefPtr<CefV8Exception> exc;
         context->Eval("alert('Could not load on_v8_context_created.rb')", ret, exc);
      }
   }

   CefRefPtr<RubyFnHandler> rubyHandler = new RubyFnHandler(mrb);
   auto ruby_fn = CefV8Value::CreateFunction("ruby", rubyHandler);
   context->GetGlobal()->SetValue(CefString("ruby"), ruby_fn, CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_NONE);
   LAMINA_LOG("Exit Context Created");
}