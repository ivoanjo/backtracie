// backtracie: Ruby gem for beautiful backtraces
// Copyright (C) 2021 Ivo Anjo <ivo@ivoanjo.me>
//
// This file is part of backtracie.
//
// backtracie is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// backtracie is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with backtracie.  If not, see <http://www.gnu.org/licenses/>.

#include "extconf.h"

#if 0
// Disabled until this is fixed to not break Windows
#include <dlfcn.h>
#endif

#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>
#include <stdio.h>

#include "backtracie_private.h"
#include "public/backtracie.h"

#define VALUE_COUNT(array) (sizeof(array) / sizeof(VALUE))
#define SAFE_NAVIGATION(function, maybe_nil)                                   \
  ((maybe_nil) != Qnil ? function(maybe_nil) : Qnil)

// non-static, used in backtracie_frames.c
VALUE backtracie_main_object_instance = Qnil;
VALUE backtracie_frame_wrapper_class = Qnil;

static ID ensure_object_is_thread_id;
static ID to_s_id;
static VALUE backtracie_module = Qnil;
static VALUE backtracie_location_class = Qnil;

static VALUE primitive_caller_locations(VALUE self);
static VALUE primitive_backtrace_locations(VALUE self, VALUE thread);
static VALUE collect_backtrace_locations(VALUE self, VALUE thread,
                                         int ignored_stack_top_frames);
inline static VALUE new_location(VALUE absolute_path, VALUE base_label,
                                 VALUE label, VALUE lineno, VALUE path,
                                 VALUE qualified_method_name,
                                 VALUE path_is_synthetic, VALUE debug);
static VALUE frame_to_location(const raw_location *raw_loc,
                               const raw_location *prev_ruby_loc);
static VALUE debug_raw_location(const raw_location *the_location);
static VALUE debug_frame(VALUE frame);
static VALUE cfunc_function_info(const raw_location *the_location);
static inline VALUE to_boolean(bool value);

BACKTRACIE_API
void Init_backtracie_native_extension(void) {
  backtracie_main_object_instance =
      rb_funcall(rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING")),
                 rb_intern("eval"), 1, rb_str_new2("self"));
  ensure_object_is_thread_id = rb_intern("ensure_object_is_thread");
  to_s_id = rb_intern("to_s");

  backtracie_module = rb_const_get(rb_cObject, rb_intern("Backtracie"));
  rb_global_variable(&backtracie_module);

  rb_define_module_function(backtracie_module, "backtrace_locations",
                            primitive_backtrace_locations, 1);

  backtracie_location_class =
      rb_const_get(backtracie_module, rb_intern("Location"));
  rb_global_variable(&backtracie_location_class);

  VALUE backtracie_primitive_module =
      rb_define_module_under(backtracie_module, "Primitive");

  rb_define_module_function(backtracie_primitive_module, "caller_locations",
                            primitive_caller_locations, 0);

  backtracie_frame_wrapper_class =
      rb_define_class_under(backtracie_module, "FrameWrapper", rb_cObject);
  // this class should only be instantiated via backtracie_frame_wrapper_new
  rb_undef_alloc_func(backtracie_frame_wrapper_class);

  // Create some classes which are used to simulate interesting scenarios in
  // tests
  backtracie_init_c_test_helpers(backtracie_module);
}

// Get array of Backtracie::Locations for a given thread; if thread is nil,
// returns for the current thread
static VALUE collect_backtrace_locations(VALUE self, VALUE thread,
                                         int ignored_stack_top_frames) {
  if (!RTEST(thread)) {
    thread = rb_thread_current();
  }

  // To maintain compatability with the Ruby thread backtrace behavior, if a
  // thread is dead, then return nil.
  if (!backtracie_is_thread_alive(thread)) {
    return Qnil;
  }

  int raw_frame_count = backtracie_frame_count_for_thread(thread);

  // Allocate memory for the raw_locations, and keep track of it on the Ruby
  // heap so it will be GC'd even if we raise.
  // Zero the frame array so our mark function doesn't get confused too.
  VALUE frame_wrapper = backtracie_frame_wrapper_new(raw_frame_count);
  raw_location *raw_frames = backtracie_frame_wrapper_frames(frame_wrapper);
  int *raw_frames_len = backtracie_frame_wrapper_len(frame_wrapper);

  for (int i = ignored_stack_top_frames; i < raw_frame_count; i++) {
    bool valid_frame = backtracie_capture_frame_for_thread(
        thread, i, &raw_frames[*raw_frames_len]);
    if (valid_frame) {
      (*raw_frames_len)++;
    }
  }

  VALUE rb_locations = rb_ary_new_capa(*raw_frames_len);
  // Iterate _backwards_ through the frames, so we can keep track of the
  // previous ruby frame for a C frame. This is required because C frames don't
  // have filenames or line numbers; we must instead use the filename/lineno of
  // the _caller_ of the function.
  raw_location *prev_ruby_loc = NULL;
  for (int i = *raw_frames_len - 1; i >= 0; i--) {
    if (raw_frames[i].is_ruby_frame) {
      prev_ruby_loc = &raw_frames[i];
    }
    VALUE rb_loc = frame_to_location(&raw_frames[i], prev_ruby_loc);
    rb_ary_store(rb_locations, i, rb_loc);
  }

  RB_GC_GUARD(frame_wrapper);
  return rb_locations;
}

static VALUE primitive_caller_locations(VALUE self) {
  // Ignore:
  // * the current stack frame (native)
  // * the Backtracie.caller_locations that called us
  // * the frame from the caller itself (since we're replicating the semantics
  // of Kernel#caller_locations)
  int ignored_stack_top_frames = 3;

  return collect_backtrace_locations(self, Qnil, ignored_stack_top_frames);
}

static VALUE primitive_backtrace_locations(VALUE self, VALUE thread) {
  rb_funcall(backtracie_module, ensure_object_is_thread_id, 1, thread);

  int ignored_stack_top_frames = 0;

  return collect_backtrace_locations(self, thread, ignored_stack_top_frames);
}

inline static VALUE new_location(VALUE absolute_path, VALUE base_label,
                                 VALUE label, VALUE lineno, VALUE path,
                                 VALUE qualified_method_name,
                                 VALUE path_is_synthetic, VALUE debug) {
  VALUE arguments[] = {
      absolute_path,         base_label,        label, lineno, path,
      qualified_method_name, path_is_synthetic, debug};
  return rb_class_new_instance(VALUE_COUNT(arguments), arguments,
                               backtracie_location_class);
}

static VALUE frame_to_location(const raw_location *raw_loc,
                               const raw_location *prev_ruby_loc) {
  // If raw_loc != prev_ruby_loc, that means this location is a cfunc, and not a
  // ruby frame; so, it doesn't _actually_ have a path. For compatability with
  // Thread#backtrace et. al., we return the frame of the previous
  // actually-a-ruby-frame location. When we do that, we set a flag
  // path_is_synthetic on the location so that interested callers can know if
  // that's the case.
  VALUE filename_abs;
  VALUE filename_rel;
  VALUE line_number;
  VALUE path_is_synthetic;
  if (prev_ruby_loc) {
    filename_abs = backtracie_frame_filename_rbstr(prev_ruby_loc, true);
    filename_rel = backtracie_frame_filename_rbstr(prev_ruby_loc, false);
    line_number = INT2NUM(backtracie_frame_line_number(prev_ruby_loc));
    path_is_synthetic = (raw_loc != prev_ruby_loc) ? Qtrue : Qfalse;

  } else {
    filename_abs = rb_str_new2("(in native code)");
    filename_rel = rb_str_dup(filename_abs);
    line_number = INT2NUM(0);
    path_is_synthetic = Qtrue;
  }

  return new_location(filename_abs, backtracie_frame_label_rbstr(raw_loc, true),
                      backtracie_frame_label_rbstr(raw_loc, false), line_number,
                      filename_rel, backtracie_frame_name_rbstr(raw_loc),
                      path_is_synthetic, debug_raw_location(raw_loc));
}

static VALUE debug_raw_location(const raw_location *the_location) {
  VALUE arguments[] = {
      ID2SYM(rb_intern("ruby_frame?")),
      /* => */ to_boolean(the_location->is_ruby_frame),
      ID2SYM(rb_intern("self_is_real_self?")),
      /* => */ to_boolean(the_location->self_is_real_self),
      ID2SYM(rb_intern("rb_profile_frames")),
      /* => */ debug_frame(backtracie_frame_for_rb_profile(the_location)),
      ID2SYM(rb_intern("self_or_self_class")),
      /* => */ the_location->self_or_self_class,
      ID2SYM(rb_intern("pc")),
      /* => */ ULONG2NUM((uintptr_t)the_location->pc),
      ID2SYM(rb_intern("cfunc_function_info")),
      /* => */ cfunc_function_info(the_location)};

  VALUE debug_hash = rb_hash_new();
  for (long unsigned int i = 0; i < VALUE_COUNT(arguments); i += 2)
    rb_hash_aset(debug_hash, arguments[i], arguments[i + 1]);
  return debug_hash;
}

static VALUE debug_frame(VALUE frame) {
  if (frame == Qnil)
    return Qnil;

  VALUE arguments[] = {ID2SYM(rb_intern("path")),
                       /* => */ rb_profile_frame_path(frame),
                       ID2SYM(rb_intern("absolute_path")),
                       /* => */ rb_profile_frame_absolute_path(frame),
                       ID2SYM(rb_intern("label")),
                       /* => */ rb_profile_frame_label(frame),
                       ID2SYM(rb_intern("base_label")),
                       /* => */ rb_profile_frame_base_label(frame),
                       ID2SYM(rb_intern("full_label")),
                       /* => */ rb_profile_frame_full_label(frame),
                       ID2SYM(rb_intern("first_lineno")),
                       /* => */ rb_profile_frame_first_lineno(frame),
                       ID2SYM(rb_intern("classpath")),
                       /* => */ rb_profile_frame_classpath(frame),
                       ID2SYM(rb_intern("singleton_method_p")),
                       /* => */ rb_profile_frame_singleton_method_p(frame),
                       ID2SYM(rb_intern("method_name")),
                       /* => */ rb_profile_frame_method_name(frame),
                       ID2SYM(rb_intern("qualified_method_name")),
                       /* => */ rb_profile_frame_qualified_method_name(frame)};

  VALUE debug_hash = rb_hash_new();
  for (long unsigned int i = 0; i < VALUE_COUNT(arguments); i += 2)
    rb_hash_aset(debug_hash, arguments[i], arguments[i + 1]);
  return debug_hash;
}

static VALUE cfunc_function_info(const raw_location *the_location) {
  return Qnil;
#if 0 // Disabled until this can be fixed up to not break Windows/macOS
  Dl_info symbol_info;
  struct Elf64_Sym *elf_symbol = 0;

  if (the_location->cfunc_function == NULL ||
    !dladdr1(the_location->cfunc_function, &symbol_info, (void**) &elf_symbol, RTLD_DL_SYMENT)) return Qnil;

  VALUE fname = symbol_info.dli_fname == NULL ? Qnil : rb_str_new2(symbol_info.dli_fname);
  VALUE sname = symbol_info.dli_sname == NULL ? Qnil : rb_str_new2(symbol_info.dli_sname);

  VALUE arguments[] = {
    ID2SYM(rb_intern("dli_fname")), /* => */ fname,
    ID2SYM(rb_intern("dli_sname")), /* => */ sname
  };

  VALUE debug_hash = rb_hash_new();
  for (long unsigned int i = 0; i < VALUE_COUNT(arguments); i += 2) rb_hash_aset(debug_hash, arguments[i], arguments[i+1]);
  return debug_hash;
#endif
}

static inline VALUE to_boolean(bool value) { return value ? Qtrue : Qfalse; }
