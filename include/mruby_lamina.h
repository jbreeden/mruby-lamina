#ifndef LAMINA_H
#define LAMINA_H
#include <map>
#include "mruby.h"

#if defined(_WIN32) || defined(_WIN64)
   #define WINDOWS
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Need a better logging solution... multiple threads/procs make this output ugly (err, useless?) at times
#ifndef LAMINA_DISABLE_LOGGING
#define LAMINA_LOG(msg) \
   std::cout << msg << " @ " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
#else
   ;
#endif

mrb_state* mrb_for_thread();
void set_mrb_for_thread(mrb_state* mrb);
mrb_value lamina_start(mrb_state* mrb, mrb_value self);
void mrb_mruby_lamina_gem_init(mrb_state* mrb);
void mrb_mruby_lamina_gem_final(mrb_state* mrb);

#ifdef __cplusplus
}
#endif

#endif /* LAMINA_H */
