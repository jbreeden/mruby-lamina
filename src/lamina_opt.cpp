#include "mruby.h"
#include "mruby/class.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/data.h"
#include "mruby/compile.h"
#include "mruby/array.h"
#include "mruby_lamina.h"
#include "lamina_opt.h"
#include <cstdio>

using namespace std;

#define GET_LAMINA_OPTIONS_IV(iv) \
   auto mrb = mrb_for_thread();\
   auto lamina_module = mrb_module_get(mrb, "Lamina");\
   mrb_value var = mrb_iv_get(mrb, mrb_obj_value(lamina_module), mrb_intern_cstr(mrb, iv));

// TODO: Type checking iv's

#ifdef __cplusplus
extern "C" {
#endif

string
lamina_opt_window_title() {
   GET_LAMINA_OPTIONS_IV("@window_title");
   if (mrb_nil_p(var)) {
      return "";
   }
   return mrb_str_to_cstr(mrb, var);
}

bool
lamina_opt_use_page_titles() {
   GET_LAMINA_OPTIONS_IV("@use_page_titles");
   return mrb_bool(var);
}

char**
lamina_opt_js_extensions() {
   GET_LAMINA_OPTIONS_IV("@js_extensions");
   int length = mrb_ary_len(mrb, var);
   auto files = (char**) malloc(sizeof(char*) * (length + 1));
   int i = 0;
   for (; i < length; ++i) {
      files[i] = mrb_str_to_cstr(mrb, mrb_ary_entry(var, i));
   }
   files[i] = NULL;
   return files;
}

string
lamina_opt_script_on_app_started() {
   GET_LAMINA_OPTIONS_IV("@script_on_app_started");
   if (mrb_nil_p(var)) {
      return "";
   }
   return mrb_str_to_cstr(mrb, var);
}

int
lamina_opt_remote_debugging_port() {
   GET_LAMINA_OPTIONS_IV("@remote_debugging_port");
   if (mrb_nil_p(var)) {
      return 0;
   }
   return mrb_int(mrb, var);
}

int
lamina_opt_server_port() {
   GET_LAMINA_OPTIONS_IV("@server_port");
   if (mrb_nil_p(var)) {
      return 0;
   }
   return mrb_int(mrb, var);
}

string
lamina_opt_lock_file() {
   GET_LAMINA_OPTIONS_IV("@lock_file");
   if (mrb_nil_p(var)) {
      return "";
   }
   return mrb_str_to_cstr(mrb, var);
}

string
lamina_opt_cache_path() {
   GET_LAMINA_OPTIONS_IV("@cache_path");
   if (mrb_nil_p(var)) {
      return "";
   }
   return mrb_str_to_cstr(mrb, var);
}

string
lamina_opt_browser_ipc_path() {
   GET_LAMINA_OPTIONS_IV("@browser_ipc_path");
   if (mrb_nil_p(var)) {
      return "";
   }
   return mrb_str_to_cstr(mrb, var);
}

string
lamina_opt_app_url() {
   GET_LAMINA_OPTIONS_IV("@url");
   if (mrb_nil_p(var)) {
      return "";
   }
   return mrb_str_to_cstr(mrb, var);
}

#ifdef __cplusplus
}
#endif