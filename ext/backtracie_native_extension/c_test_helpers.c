#include "backtracie_private.h"
#include "public/backtracie.h"

#include <ruby.h>
#include <ruby/thread.h>

static VALUE backtracie_backtrace_from_thread(VALUE self);
static VALUE backtracie_backtrace_from_thread_cthread(void *ctx);
static VALUE stdlib_backtrace_from_thread(VALUE self);
static VALUE stdlib_backtrace_from_thread_cthread(void *ctx);
static VALUE backtracie_backtrace_from_empty_thread(VALUE self);
static VALUE backtracie_backtrace_from_empty_thread_cthread(void *ctx);

void backtracie_init_c_test_helpers(VALUE backtracie_module) {
  VALUE test_helpers_mod =
      rb_define_module_under(backtracie_module, "TestHelpers");
  rb_define_singleton_method(test_helpers_mod,
                             "backtracie_backtrace_from_thread",
                             backtracie_backtrace_from_thread, 0);
  rb_define_singleton_method(test_helpers_mod, "stdlib_backtrace_from_thread",
                             stdlib_backtrace_from_thread, 0);
  rb_define_singleton_method(test_helpers_mod,
                             "backtracie_backtrace_from_empty_thread",
                             backtracie_backtrace_from_empty_thread, 0);
}

static VALUE backtracie_backtrace_from_thread(VALUE self) {
  VALUE th = rb_thread_create(backtracie_backtrace_from_thread_cthread, NULL);
  return rb_funcall(th, rb_intern("value"), 0);
}

static VALUE backtracie_backtrace_from_thread_cthread(void *ctx) {
  VALUE backtracie_mod = rb_const_get(rb_cObject, rb_intern("Backtracie"));
  return rb_funcall(backtracie_mod, rb_intern("backtrace_locations"), 1,
                    rb_thread_current());
}

static VALUE stdlib_backtrace_from_thread(VALUE self) {
  VALUE th = rb_thread_create(stdlib_backtrace_from_thread_cthread, NULL);
  return rb_funcall(th, rb_intern("value"), 0);
}

static VALUE stdlib_backtrace_from_thread_cthread(void *ctx) {
  return rb_funcall(rb_thread_current(), rb_intern("backtrace_locations"), 0);
}

static VALUE backtracie_backtrace_from_empty_thread(VALUE self) {
  VALUE backtracie_mod = rb_const_get(rb_cObject, rb_intern("Backtracie"));
  VALUE th =
      rb_thread_create(backtracie_backtrace_from_empty_thread_cthread, NULL);
  VALUE bt =
      rb_funcall(backtracie_mod, rb_intern("backtrace_locations"), 1, th);
  rb_thread_kill(th);
  rb_funcall(th, rb_intern("join"), 0);
  return bt;
}

static VALUE backtracie_backtrace_from_empty_thread_cthread(void *ctx) {
  // yield the GVL without creating a frame of any kind
  rb_thread_sleep(-1);
  return Qnil;
}
