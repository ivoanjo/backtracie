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

#include <stdbool.h>

#include "ruby/ruby.h"
#include "ruby/debug.h"

#include "extconf.h"

#include "ruby_shards.h"

#define MAX_STACK_DEPTH 2000 // FIXME: Need to handle when this is not enough

#define VALUE_COUNT(array) (sizeof(array) / sizeof(VALUE))
#define SAFE_NAVIGATION(function, maybe_nil) ((maybe_nil) != Qnil ? function(maybe_nil) : Qnil)

static VALUE main_object_instance = Qnil;
static ID ensure_object_is_thread_id;
static ID to_s_id;
static VALUE backtracie_module = Qnil;
static VALUE backtracie_location_class = Qnil;

static VALUE primitive_caller_locations(VALUE self);
static VALUE primitive_backtrace_locations(VALUE self, VALUE thread);
static VALUE collect_backtrace_locations(VALUE self, VALUE thread, int ignored_stack_top_frames);
inline static VALUE new_location(VALUE absolute_path, VALUE base_label, VALUE label, VALUE lineno, VALUE path, VALUE qualified_method_name, VALUE debug);
static VALUE ruby_frame_to_location(raw_location *the_location);
static VALUE qualified_method_name_for_location(raw_location *the_location);
static VALUE cfunc_frame_to_location(raw_location *the_location, raw_location *last_ruby_location);
static VALUE frame_from_location(raw_location *the_location);
static VALUE qualified_method_name_for_block(raw_location *the_location);
static VALUE qualified_method_name_from_self(raw_location *the_location);
static bool is_self_class_singleton(raw_location *the_location);
static bool is_defined_class_a_refinement(raw_location *the_location);
static VALUE debug_raw_location(raw_location *the_location);
static VALUE debug_frame(VALUE frame);
static inline VALUE to_boolean(bool value) ;

void Init_backtracie_native_extension(void) {
  main_object_instance = backtracie_rb_vm_top_self();
  ensure_object_is_thread_id = rb_intern("ensure_object_is_thread");
  to_s_id = rb_intern("to_s");

  backtracie_module = rb_const_get(rb_cObject, rb_intern("Backtracie"));
  rb_global_variable(&backtracie_module);

  rb_define_module_function(backtracie_module, "backtrace_locations", primitive_backtrace_locations, 1);

  backtracie_location_class = rb_const_get(backtracie_module, rb_intern("Location"));
  rb_global_variable(&backtracie_location_class);

  VALUE backtracie_primitive_module = rb_define_module_under(backtracie_module, "Primitive");

  rb_define_module_function(backtracie_primitive_module, "caller_locations", primitive_caller_locations, 0);
}

// Get array of Backtracie::Locations for a given thread; if thread is nil, returns for the current thread
static VALUE collect_backtrace_locations(VALUE self, VALUE thread, int ignored_stack_top_frames) {
  int stack_depth = 0;
  raw_location raw_locations[MAX_STACK_DEPTH];

  if (thread == Qnil) {
    // Get for own thread
    stack_depth = backtracie_rb_profile_frames(MAX_STACK_DEPTH, raw_locations);
  } else {
    stack_depth = backtracie_rb_profile_frames_for_thread(thread, MAX_STACK_DEPTH, raw_locations);
    if (stack_depth == 0 && !backtracie_is_thread_alive(thread)) return Qnil;
  }

  VALUE locations = rb_ary_new_capa(stack_depth - ignored_stack_top_frames);

  // MRI does not give us the path or line number for frames implemented using C code. The convention in
  // Kernel#caller_locations is to instead use the path and line number of the last Ruby frame seen.
  // Thus, we keep that frame here to able to replicate that behavior.
  // (This is why we also iterate the frames array backwards below -- so that it's easier to keep the last_ruby_frame)
  raw_location *last_ruby_location = 0;

  for (int i = stack_depth - 1; i >= ignored_stack_top_frames; i--) {
    VALUE location = Qnil;

    if (raw_locations[i].is_ruby_frame) {
      last_ruby_location = &raw_locations[i];
      location = ruby_frame_to_location(&raw_locations[i]);
    } else {
      location = cfunc_frame_to_location(&raw_locations[i], last_ruby_location);
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

  return collect_backtrace_locations(self, Qnil, ignored_stack_top_frames);
}

static VALUE primitive_backtrace_locations(VALUE self, VALUE thread) {
  rb_funcall(backtracie_module, ensure_object_is_thread_id, 1, thread);

  int ignored_stack_top_frames = 0;

  return collect_backtrace_locations(self, thread, ignored_stack_top_frames);
}

inline static VALUE new_location(
  VALUE absolute_path,
  VALUE base_label,
  VALUE label,
  VALUE lineno,
  VALUE path,
  VALUE qualified_method_name,
  VALUE debug
) {
  VALUE arguments[] = { absolute_path, base_label, label, lineno, path, qualified_method_name, debug };
  return rb_class_new_instance(VALUE_COUNT(arguments), arguments, backtracie_location_class);
}

static VALUE ruby_frame_to_location(raw_location *the_location) {
  VALUE frame = frame_from_location(the_location);

  return new_location(
    rb_profile_frame_absolute_path(frame),
    rb_profile_frame_base_label(frame),
    rb_profile_frame_label(the_location->iseq),
    INT2FIX(the_location->line_number),
    rb_profile_frame_path(frame),
    qualified_method_name_for_location(the_location),
    debug_raw_location(the_location)
  );
}

static VALUE qualified_method_name_for_location(raw_location *the_location) {
  VALUE frame = frame_from_location(the_location);
  VALUE defined_class = backtracie_defined_class(the_location);
  VALUE qualified_method_name = Qnil;

  if (is_defined_class_a_refinement(the_location)) {
    qualified_method_name = backtracie_refinement_name(the_location);
    rb_str_concat(qualified_method_name, rb_str_new2("#"));
    rb_str_concat(qualified_method_name, rb_profile_frame_label(frame));
  } else if (is_self_class_singleton(the_location)) {
    qualified_method_name = qualified_method_name_from_self(the_location);
  } else if (backtracie_iseq_is_block(the_location) || backtracie_iseq_is_eval(the_location)) {
    qualified_method_name = qualified_method_name_for_block(the_location);
  } else if (defined_class != Qnil && rb_mod_name(defined_class) == Qnil) {
    // Instance of an anonymous class. Let's find it a name
    VALUE superclass = defined_class;
    VALUE superclass_name = Qnil;
    do {
      superclass = RCLASS_SUPER(superclass);
      superclass_name = rb_mod_name(superclass);
    } while (superclass_name == Qnil);

    qualified_method_name = rb_str_new2("");
    rb_str_concat(qualified_method_name, superclass_name);
    rb_str_concat(qualified_method_name, rb_str_new2("$anonymous#"));
    rb_str_concat(qualified_method_name, rb_profile_frame_base_label(frame_from_location(the_location)));
  } else {
    qualified_method_name = backtracie_rb_profile_frame_qualified_method_name(frame);

    if (qualified_method_name == Qnil) {
      qualified_method_name = qualified_method_name_from_self(the_location);
    }
  }

  if (backtracie_iseq_is_block(the_location) || backtracie_iseq_is_eval(the_location)) {
    rb_str_concat(qualified_method_name, rb_str_new2("{block}"));
  }

  return qualified_method_name;
}

static VALUE cfunc_frame_to_location(raw_location *the_location, raw_location *last_ruby_location) {
  VALUE last_ruby_frame =
    last_ruby_location != 0 ? frame_from_location(last_ruby_location) : Qnil;

  // Replaces label and base_label in cfuncs
  VALUE method_name = backtracie_rb_profile_frame_method_name(the_location->callable_method_entry);

  return new_location(
    SAFE_NAVIGATION(rb_profile_frame_absolute_path, last_ruby_frame),
    method_name,
    method_name,
    last_ruby_location != 0 ? INT2FIX(last_ruby_location->line_number) : Qnil,
    SAFE_NAVIGATION(rb_profile_frame_path, last_ruby_frame),
    backtracie_rb_profile_frame_qualified_method_name(the_location->callable_method_entry),
    debug_raw_location(the_location)
  );
}

static VALUE frame_from_location(raw_location *the_location) {
  return \
    the_location->should_use_iseq ||
    // This one is somewhat weird, but the regular MRI Ruby APIs seem to pick the iseq for evals as well
    backtracie_iseq_is_eval(the_location) ?
      the_location->iseq : the_location->callable_method_entry;
}

static VALUE qualified_method_name_for_block(raw_location *the_location) {
  VALUE class_name = backtracie_rb_profile_frame_classpath(the_location->callable_method_entry);
  VALUE method_name = backtracie_called_id(the_location);
  VALUE is_singleton_method = rb_profile_frame_singleton_method_p(the_location->iseq);

  VALUE name = rb_str_new2("");
  rb_str_concat(name, class_name);
  rb_str_concat(name, is_singleton_method ? rb_str_new2(".") : rb_str_new2("#"));
  rb_str_concat(name, rb_sym2str(method_name));

  return name;
}

static VALUE qualified_method_name_from_self(raw_location *the_location) {
  if (the_location->self == Qnil) return Qnil;

  VALUE self_class = rb_class_of(the_location->self);
  bool is_self_class_singleton = FL_TEST(self_class, FL_SINGLETON);

  VALUE name = rb_str_new2("");
  if (is_self_class_singleton) {
    if (the_location->self == main_object_instance) {
      rb_str_concat(name, rb_str_new2("Object$<main>#"));
    } else {
      // Crawl up the hierarchy to find a real class
      VALUE the_class = rb_class_real(self_class);

      // This is similar to BetterBacktraceHelper's self_class_module_or_class?
      // If the real class of this object is the actual class Class or the class Module,
      // it means that this is a method being directly called on a given Class/Module,
      // e.g. Kernel.puts or BasicObject.name. In this case, Ruby already sets the name
      // correctly, so we just delegate.
      if (the_class == rb_cModule || the_class == rb_cClass) {
        // Is the class/module is anonymous?
        if (rb_mod_name(the_location->self) == Qnil) {
          rb_str_concat(name, rb_funcall(the_class, to_s_id, 0));
          rb_str_concat(name, rb_str_new2("$singleton."));
        } else {
          return SAFE_NAVIGATION(backtracie_rb_profile_frame_qualified_method_name, the_location->callable_method_entry);
        }
      } else {
        rb_str_concat(name, rb_funcall(the_class, to_s_id, 0));
        rb_str_concat(name, rb_str_new2("$singleton#"));
      }
    }
  } else {
    // Not very sure if this branch of the if is ever reached, and if it would be for a instance or static call, so
    // let's just have these defaults and revisit as needed
    rb_str_concat(name, rb_funcall(self_class, to_s_id, 0));
    rb_str_concat(name, rb_str_new2("#"));
  }

  if (backtracie_iseq_is_block(the_location) || backtracie_iseq_is_eval(the_location)) {
    // Nothing to do, {block} will be appended in qualified_method_name_for_location which called us
  } else {
    rb_str_concat(name, rb_profile_frame_base_label(frame_from_location(the_location)));
  }

  return name;
}

static bool is_self_class_singleton(raw_location *the_location) {
  return the_location->self != Qnil && FL_TEST(rb_class_of(the_location->self), FL_SINGLETON);
}

static bool is_defined_class_a_refinement(raw_location *the_location) {
  VALUE defined_class = backtracie_defined_class(the_location);

  return defined_class != Qnil && FL_TEST(rb_class_of(defined_class), RMODULE_IS_REFINEMENT);
}

static VALUE debug_raw_location(raw_location *the_location) {
  VALUE self_class = SAFE_NAVIGATION(rb_class_of, the_location->self);

  VALUE arguments[] = {
    ID2SYM(rb_intern("ruby_frame?"))  ,             /* => */ to_boolean(the_location->is_ruby_frame),
    ID2SYM(rb_intern("should_use_iseq")),           /* => */ to_boolean(the_location->should_use_iseq),
    ID2SYM(rb_intern("vm_method_type")),            /* => */ INT2FIX(the_location->vm_method_type),
    ID2SYM(rb_intern("line_number")),               /* => */ INT2FIX(the_location->line_number),
    ID2SYM(rb_intern("called_id")),                 /* => */ backtracie_called_id(the_location),
    // TODO: On Ruby < 3.0, running be pry -e 'require "backtracie"; Backtracie.caller_locations' with this being
    // exposed causes a VM segfault inside the pretty printing code
    // ID2SYM(rb_intern("defined_class")),             /* => */ backtracie_defined_class(the_location),
    ID2SYM(rb_intern("defined_class_refinement?")), /* => */ to_boolean(is_defined_class_a_refinement(the_location)),
    ID2SYM(rb_intern("self_class")),                /* => */ self_class,
    ID2SYM(rb_intern("real_class")),                /* => */ SAFE_NAVIGATION(rb_class_real, self_class),
    ID2SYM(rb_intern("self")),                      /* => */ the_location->self,
    ID2SYM(rb_intern("self_class_singleton?")),     /* => */ to_boolean(is_self_class_singleton(the_location)),
    ID2SYM(rb_intern("iseq_is_block?")),            /* => */ to_boolean(backtracie_iseq_is_block(the_location)),
    ID2SYM(rb_intern("iseq_is_eval?")),             /* => */ to_boolean(backtracie_iseq_is_eval(the_location)),
    ID2SYM(rb_intern("iseq")),                      /* => */ debug_frame(the_location->iseq),
    ID2SYM(rb_intern("callable_method_entry")),     /* => */ debug_frame(the_location->callable_method_entry)
  };

  VALUE debug_hash = rb_hash_new();
  // FIXME: Hack, need to actually fix this
  rb_hash_bulk_insert(VALUE_COUNT(arguments), arguments, debug_hash);
  return debug_hash;
}

static VALUE debug_frame(VALUE frame) {
  if (frame == Qnil) return Qnil;

  VALUE arguments[] = {
    ID2SYM(rb_intern("path")),                  /* => */ rb_profile_frame_path(frame),
    ID2SYM(rb_intern("absolute_path")),         /* => */ rb_profile_frame_absolute_path(frame),
    ID2SYM(rb_intern("label")),                 /* => */ rb_profile_frame_label(frame),
    ID2SYM(rb_intern("base_label")),            /* => */ rb_profile_frame_base_label(frame),
    ID2SYM(rb_intern("full_label")),            /* => */ rb_profile_frame_full_label(frame),
    ID2SYM(rb_intern("first_lineno")),          /* => */ rb_profile_frame_first_lineno(frame),
    ID2SYM(rb_intern("classpath")),             /* => */ backtracie_rb_profile_frame_classpath(frame),
    ID2SYM(rb_intern("singleton_method_p")),    /* => */ rb_profile_frame_singleton_method_p(frame),
    ID2SYM(rb_intern("method_name")),           /* => */ rb_profile_frame_method_name(frame),
    ID2SYM(rb_intern("qualified_method_name")), /* => */ backtracie_rb_profile_frame_qualified_method_name(frame)
  };

  VALUE debug_hash = rb_hash_new();
  // FIXME: Hack, need to actually fix this
  rb_hash_bulk_insert(VALUE_COUNT(arguments), arguments, debug_hash);
  return debug_hash;
}

static inline VALUE to_boolean(bool value) {
  return value ? Qtrue : Qfalse;
}
