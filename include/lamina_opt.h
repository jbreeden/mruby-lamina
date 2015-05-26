#ifndef LAMINA_OPTIONS_H
#define LAMINA_OPTIONS_H

#include <stdlib.h>
#include <string>
#include <list>
#include <vector>
#include "mruby.h"

#ifdef __cplusplus
extern "C" {
#endif

   // TODO: Return char*, not string
   std::string lamina_opt_window_title();
   bool lamina_opt_use_page_titles();
   char** lamina_opt_js_extensions();
   std::string lamina_opt_script_on_app_started();
   int lamina_opt_remote_debugging_port();
   int lamina_opt_server_port();
   std::string lamina_opt_lock_file();
   std::string lamina_opt_cache_path();
   std::string lamina_opt_browser_ipc_path();
   std::string lamina_opt_app_url();

#ifdef __cplusplus
}
#endif

#endif /* LAMINA_OPTIONS_H */