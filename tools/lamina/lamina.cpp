#include <iostream>
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby_lamina.h"
#include "mruby/string.h"

extern int global_argc;
extern char** global_argv;

int main(int argc, char** argv)
{
#ifndef WINDOWS
   global_argc = argc;
   global_argv = argv;
#endif

   mrb_state* mrb = mrb_open();
   set_mrb_for_thread(mrb);

   mrbc_context* context = mrbc_context_new(mrb);
   context->filename = "lamina_main.rb";

   FILE* startup_script = fopen("lamina_main.rb", "r");
   if (startup_script != NULL) {
      mrb_load_file_cxt(mrb, startup_script, context);
      if (mrb->exc) {
         LAMINA_LOG("!!! Error !!! " << mrb_str_to_cstr(mrb, mrb_funcall(mrb, mrb_obj_value(mrb->exc), "to_s", 0)));
      }
      return 0;
   }
   else {
      return 1;
   }
}
