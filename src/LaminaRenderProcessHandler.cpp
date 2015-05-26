#define _CRT_SECURE_NO_DEPRECATE

#include <iostream>
#include "stdio.h"
#include "errno.h"
#include "mruby.h"
#include "mruby_lamina.h"
#include "mruby/compile.h"
#include "mruby_cef.h"
#include "lamina_opt.h"
#include "LaminaRenderProcessHandler.h"
#include "ruby_fn_handler.h"

LaminaRenderProcessHandler::LaminaRenderProcessHandler() {}

void LaminaRenderProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
   auto mrb = mrb_for_thread();
   auto files = lamina_opt_js_extensions();

   for (int i = 0; files[i] != NULL; ++i) {
      FILE* script = NULL;
      char* buffer = NULL;

      const char* script_file_name = files[i];
      bool is_ruby = 0 == strcmp(script_file_name + strlen(script_file_name) - 3, ".rb");
      bool is_javascript = 0 == strcmp(script_file_name + strlen(script_file_name) - 3, ".js");

      LAMINA_LOG("LaminaRenderProcessHandler: Loading extension file " << script_file_name);

      script = fopen(script_file_name, "r");

      if (script == NULL) {
         LAMINA_LOG("LaminaRenderProcessHandler: Error loading v8 extensions from file " << script_file_name);
      }
      else if (is_ruby) {
         mrb_load_file(mrb, script);
         if (mrb->exc) {
            LAMINA_LOG("LaminaRenderProcessHandler: Error loading ruby extesion. "
               << mrb_str_to_cstr(
                     mrb, 
                     mrb_funcall(mrb, mrb_obj_value(mrb->exc), "to_s", 0)
                  )
            );
            mrb->exc = NULL;
         }
      }
      else if (is_javascript) {
         CefRefPtr<CefV8Value> ret;
         CefRefPtr<CefV8Exception> exc;

         fseek(script, 0, SEEK_END);
         int length = ftell(script);
         rewind(script);

         char* buffer = (char*)calloc(1, length + 1);
         if (!buffer) {
            LAMINA_LOG("LaminaRenderProcessHandler: Error creating read buffer for file " << script_file_name);
            goto NEXT;
         }

         if (length != fread(buffer, 1, length, script)) {
            LAMINA_LOG("LaminaRenderProcessHandler: Error reading file " << script_file_name);
            goto NEXT;
         }

         context->Eval(buffer, ret, exc);

         if (exc.get() != NULL) {
            LAMINA_LOG("LaminaRenderProcessHandler: Error on line " 
               << exc->GetLineNumber() 
               << " of file " 
               << script_file_name 
               << ". " 
               << exc->GetMessageA().ToString());
         }
      }

   NEXT:
      if (script) fclose(script);
      if (buffer) free(buffer);
      free(files[i]);
   }
   free(files);

   CefRefPtr<RubyFnHandler> rubyHandler = new RubyFnHandler(mrb);
   auto ruby_fn = CefV8Value::CreateFunction("ruby", rubyHandler);
   context->GetGlobal()->SetValue(CefString("ruby"), ruby_fn, CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_NONE);
}