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

#include "ruby/ruby.h"
#include "ruby/debug.h"

#include "extconf.h"

#include "ruby_3.0.0.h"

// Constants

#define MAX_STACK_DEPTH 2000 // FIXME: Need to handle when this is not enough

// Globals

static VALUE backtracie_module = Qnil;
static VALUE backtracie_location_class = Qnil;

// Function headers

static VALUE primitive_caller_locations(VALUE self);
static VALUE primitive_backtrace_locations(VALUE self, VALUE thread);
static VALUE caller_locations(VALUE self, VALUE thread, int ignored_stack_top_frames);
inline static VALUE new_location(VALUE absolute_path, VALUE base_label, VALUE label, VALUE lineno, VALUE path, VALUE debug);
static bool is_ruby_frame(VALUE ruby_frame);
static VALUE ruby_frame_to_location(VALUE frame, VALUE last_ruby_line, VALUE correct_label);
static VALUE cfunc_frame_to_location(VALUE frame, VALUE last_ruby_frame, VALUE last_ruby_line);
static VALUE debug_frame(VALUE frame, VALUE type, VALUE correct_label);

// Macros

#define VALUE_COUNT(array) (sizeof(array) / sizeof(VALUE))

void Init_backtracie_native_extension(void) {
  backtracie_module = rb_const_get(rb_cObject, rb_intern("Backtracie"));
  rb_global_variable(&backtracie_module);

  rb_define_module_function(backtracie_module, "backtrace_locations", primitive_backtrace_locations, 1);

  // We need to keep a reference to Backtracie::Locations around, to create new instances
  backtracie_location_class = rb_const_get(backtracie_module, rb_intern("Location"));
  rb_global_variable(&backtracie_location_class);

  VALUE backtracie_primitive_module = rb_define_module_under(backtracie_module, "Primitive");

  rb_define_module_function(backtracie_primitive_module, "caller_locations", primitive_caller_locations, 0);
}

// Get array of Backtracie::Locations for a given thread; if thread is nil, returns for the current thread
static VALUE caller_locations(VALUE self, VALUE thread, int ignored_stack_top_frames) {
  int stack_depth = 0;
  VALUE frames[MAX_STACK_DEPTH];
  VALUE correct_labels[MAX_STACK_DEPTH];
  int lines[MAX_STACK_DEPTH];

  if (thread == Qnil) {
    // Get for own thread
    stack_depth = modified_rb_profile_frames(0, MAX_STACK_DEPTH, frames, correct_labels, lines);
  } else {
    stack_depth = modified_rb_profile_frames_for_thread(thread, 0, MAX_STACK_DEPTH, frames, correct_labels, lines);
  }

  // Ignore the last frame -- seems to be an uninteresting VM frame. MRI itself seems to ignore the last frame in
  // the implementation of backtrace_collect()
  int ignored_stack_bottom_frames = 1;

  stack_depth -= ignored_stack_bottom_frames;

  VALUE locations = rb_ary_new_capa(stack_depth - ignored_stack_top_frames);

  // MRI does not give us the path or line number for frames implemented using C code. The convention in
  // Kernel#caller_locations is to instead use the path and line number of the last Ruby frame seen.
  // Thus, we keep that frame here to able to replicate that behavior.
  // (This is why we also iterate the frames array backwards below -- so that it's easier to keep the last_ruby_frame)
  VALUE last_ruby_frame = Qnil;
  VALUE last_ruby_line = Qnil;

  for (int i = stack_depth - 1; i >= ignored_stack_top_frames; i--) {
    VALUE frame = frames[i];
    int line = lines[i];

    VALUE location = Qnil;

    if (is_ruby_frame(frame)) {
      last_ruby_frame = frame;
      last_ruby_line = INT2FIX(line);

      location = ruby_frame_to_location(frame, last_ruby_line, correct_labels[i]);
    } else {
      location = cfunc_frame_to_location(frame, last_ruby_frame, last_ruby_line);
    }

    rb_ary_store(locations, i - ignored_stack_top_frames, location);
  }

  return locations;
}

static VALUE primitive_caller_locations(VALUE self) {
  // Ignore:
  // * the current stack frame (native)
  // * the Backtracie.caller_locations that called us
  // * the frame from the caller itself (since we're replicating the semantics of Kernel#caller_locations)
  int ignored_stack_top_frames = 3;

  return caller_locations(self, Qnil, ignored_stack_top_frames);
}

static VALUE primitive_backtrace_locations(VALUE self, VALUE thread) {
  rb_funcall(backtracie_module, rb_intern("ensure_object_is_thread"), 1, thread);

  int ignored_stack_top_frames = 0;

  return caller_locations(self, thread, ignored_stack_top_frames);
}

inline static VALUE new_location(VALUE absolute_path, VALUE base_label, VALUE label, VALUE lineno, VALUE path, VALUE debug) {
  VALUE arguments[] = { absolute_path, base_label, label, lineno, path, debug };
  return rb_class_new_instance(VALUE_COUNT(arguments), arguments, backtracie_location_class);
}

static bool is_ruby_frame(VALUE frame) {
  VALUE absolute_path = rb_profile_frame_absolute_path(frame);

  return (rb_profile_frame_path(frame) != Qnil || absolute_path != Qnil) &&
    (rb_funcall(absolute_path, rb_intern("=="), 1, rb_str_new2("<cfunc>")) == Qfalse);
}

static VALUE ruby_frame_to_location(VALUE frame, VALUE last_ruby_line, VALUE correct_label) {
  return new_location(
    rb_profile_frame_absolute_path(frame),
    rb_profile_frame_base_label(frame),
    rb_profile_frame_label(correct_label),
    last_ruby_line,
    rb_profile_frame_path(frame),
    debug_frame(frame, rb_str_new2("ruby_frame"), correct_label)
  );
}

static VALUE cfunc_frame_to_location(VALUE frame, VALUE last_ruby_frame, VALUE last_ruby_line) {
  VALUE method_name = rb_profile_frame_method_name(frame); // Replaces label and base_label in cfuncs

  return new_location(
    last_ruby_frame != Qnil ? rb_profile_frame_absolute_path(last_ruby_frame) : Qnil,
    method_name,
    method_name,
    last_ruby_line,
    last_ruby_frame != Qnil ? rb_profile_frame_path(last_ruby_frame) : Qnil,
    debug_frame(frame, rb_str_new2("cfunc_frame"), Qnil)
  );
}

// Used to dump all the things we get from the rb_profile_frames API, for debugging
static VALUE debug_frame(VALUE frame, VALUE type, VALUE correct_label) {
  VALUE arguments[] = {
    rb_profile_frame_path(frame),
    rb_profile_frame_absolute_path(frame),
    rb_profile_frame_label(frame),
    rb_profile_frame_base_label(frame),
    rb_profile_frame_full_label(frame),
    rb_profile_frame_first_lineno(frame),
    rb_profile_frame_classpath(frame),
    rb_profile_frame_singleton_method_p(frame),
    rb_profile_frame_method_name(frame),
    rb_profile_frame_qualified_method_name(frame),
    type,
    correct_label != Qnil ? debug_frame(correct_label, Qnil, Qnil) : Qnil,
  };
  return rb_ary_new_from_values(VALUE_COUNT(arguments), arguments);
}
