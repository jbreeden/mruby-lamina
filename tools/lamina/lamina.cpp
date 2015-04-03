#include <stdio.h>
#include "Windows.h"
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby_lamina.h"

int main(int argc, char** argv)
{
   auto mrb = mrb_open();
   set_mrb_for_thread(mrb);

   FILE* startup_script = fopen("lamina_main.rb", "r");
   if (startup_script != NULL) {
      mrb_load_file(mrb, startup_script);
      return 0;
   }
   else {
      return 1;
   }
}
