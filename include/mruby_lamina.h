#ifndef LAMINA_H
#define LAMINA_H
#include <map>
#include "mruby.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAMINA_LOG(msg) printf("%s\n", msg);

mrb_state* mrb_for_thread();
void set_mrb_for_thread(mrb_state* mrb);
mrb_value lamina_start(mrb_state* mrb, mrb_value self);
void mrb_mruby_lamina_gem_init(mrb_state* mrb);
void mrb_mruby_lamina_gem_final(mrb_state* mrb);

#ifdef __cplusplus
}
#endif

#endif /* LAMINA_H */
