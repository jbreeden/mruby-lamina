#ifndef LAMINA_H
#define LAMINA_H
#include <map>
#include <thread>
#include "mruby.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAMINA_LOG(msg)\
   do {\
      FILE* log = fopen("lamina_log.txt", "a");\
      size_t thread_hash = std::this_thread::get_id().hash();\
      fprintf(log, "[%d] %s\n", thread_hash, msg);\
      fclose(log);\
   } while (0)

mrb_state* mrb_for_thread();
void set_mrb_for_thread(mrb_state* mrb);
mrb_value lamina_start(mrb_state* mrb, mrb_value self);
void mrb_mruby_lamina_gem_init(mrb_state* mrb);
void mrb_mruby_lamina_gem_final(mrb_state* mrb);

#ifdef __cplusplus
}
#endif

#endif /* LAMINA_H */