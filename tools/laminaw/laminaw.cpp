#include <stdio.h>
#include "Windows.h"
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby_lamina.h"

HINSTANCE app_handle;

int APIENTRY WinMain(HINSTANCE hInstance,
   HINSTANCE hPrevInstance,
   LPSTR    lpCmdLine,
   int       nCmdShow)
{
   char msg[20];
   sprintf(msg, "Argc: %d", __argc);
   LAMINA_LOG(msg);

   app_handle = hInstance;

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
