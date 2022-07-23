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

// -----------------------------------------------------------------------------
// The file below has modified versions of code extracted from the Ruby project.
// The Ruby project copyright and license follow:
// -----------------------------------------------------------------------------

// Ruby is copyrighted free software by Yukihiro Matsumoto <matz@netlab.jp>.
// You can redistribute it and/or modify it under either the terms of the
// 2-clause BSDL (see the file BSDL), or the conditions below:

// 1. You may make and give away verbatim copies of the source form of the
//    software without restriction, provided that you duplicate all of the
//    original copyright notices and associated disclaimers.

// 2. You may modify your copy of the software in any way, provided that
//    you do at least ONE of the following:

//    a. place your modifications in the Public Domain or otherwise
//       make them Freely Available, such as by posting said
//       modifications to Usenet or an equivalent medium, or by allowing
//       the author to include your modifications in the software.

//    b. use the modified software only within your corporation or
//       organization.

//    c. give non-standard binaries non-standard names, with
//       instructions on where to get the original software distribution.

//    d. make other distribution arrangements with the author.

// 3. You may distribute the software in object code or binary form,
//    provided that you do at least ONE of the following:

//    a. distribute the binaries and library files of the software,
//       together with instructions (in the manual page or equivalent)
//       on where to get the original distribution.

//    b. accompany the distribution with the machine-readable source of
//       the software.

//    c. give non-standard binaries non-standard names, with
//       instructions on where to get the original software distribution.

//    d. make other distribution arrangements with the author.

// 4. You may modify and include the part of the software into any other
//    software (possibly commercial).  But some files in the distribution
//    are not written by the author, so that they are not under these terms.

//    For the list of those files and their copying conditions, see the
//    file LEGAL.

// 5. The scripts and library files supplied as input to or produced as
//    output from the software do not automatically fall under the
//    copyright of the software, but belong to whomever generated them,
//    and may be sold commercially, and may be aggregated with this
//    software.

// 6. THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
//    IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
//    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//    PURPOSE.

// -----------------------------------------------------------------------------

// backtracie_frames.c contains a number of borrowed functions from the MRI Ruby
// (usually 3.0.0) source tree:
// * A few were copy-pasted verbatim, and are dependencies for other functions
// * A few were copy-pasted and then changed so we can add features and fixes
// * A few were copy-pasted verbatim, with the objective of backporting
//   their 3.0.0 behavior to earlier Ruby versions.
//
// `git blame` usually documents which functions were added for what reason.

// Note that since the RUBY_MJIT_HEADER is a very special header, meant for
// internal use only, it has a number of quirks:
//
// 1. I've seen a few segfaults when trying to call back into original Ruby
// functions. E.g. even if the API is used
//    correctly, just the mere inclusion of RUBY_MJIT_HEADER causes usage to
//    crash. Thus, as much as possible, it's better to define functions OUTSIDE
//    this file.
//
// 2. On Windows, I've observed "multiple definition of `something...'" (such as
// `rb_vm_ep_local_ep') whenever there
//    are multiple files in the codebase that include the RUBY_MJIT_HEADER.
//    It looks like (some?) Windows versions of Ruby define a bunch of functions
//    in the RUBY_MJIT_HEADER itself without marking them as "static" (e.g. not
//    visible to the outside of the file), and thus the linker then complains
//    when linking together several files which all have these non-private
//    symbols. One possible hacky solution suggested by the internets is to use
//    the "-Wl,-allow-multiple-definition" linker flags to ignore this problem;
//    instead I've chosen to implement all usage of the RUBY_MJIT_HEADER on this
//    file -- no other file in backtracie shall include RUBY_MJIT_HEADER. It's a
//    simpler approach, and hopefully avoids any problems.

#include "extconf.h"

#ifndef PRE_MJIT_RUBY
#ifndef RUBY_MJIT_HEADER_INCLUDED
#define RUBY_MJIT_HEADER_INCLUDED
#include RUBY_MJIT_HEADER
#endif
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef PRE_MJIT_RUBY
// The order of includes here is very important in older versions of Ruby
// clang-format off
#include <ruby.h>
#include <vm_core.h>
#include <method.h>
#include <iseq.h>
#include <regenc.h>
// clang-format on
#endif

#include "backtracie_private.h"
#include "public/backtracie.h"
#include "strbuilder.h"

#ifdef PRE_EXECUTION_CONTEXT
// The thread and its execution context were separated on Ruby 2.5; prior to
// that, everything was part of the thread
#define rb_execution_context_t rb_thread_t
#endif

#ifdef PRE_VM_ENV_RENAMES
#define VM_ENV_LOCAL_P VM_EP_LEP_P
#define VM_ENV_PREV_EP VM_EP_PREV_EP
#define VM_ENV_DATA_INDEX_ME_CREF -1
#define VM_FRAME_RUBYFRAME_P(cfp) RUBY_VM_NORMAL_ISEQ_P(cfp->iseq)
#endif

// This is managed in backtracie.c
extern VALUE backtracie_main_object_instance;
extern VALUE backtracie_frame_wrapper_class;

static void mod_to_s_anon(VALUE klass, strbuilder_t *strout);
static void mod_to_s_refinement(VALUE klass, strbuilder_t *strout);
static void mod_to_s_singleton(VALUE klass, strbuilder_t *strout);
static void mod_to_s(VALUE klass, strbuilder_t *strout);
static void method_qualifier(const raw_location *loc, strbuilder_t *strout);
static void method_name(const raw_location *loc, strbuilder_t *strout);
static bool frame_filename(const raw_location *loc, size_t loc_len,
                           bool absolute, strbuilder_t *strout);
static bool iseq_path(const rb_iseq_t *iseq, bool absolute,
                      strbuilder_t *strout);
static int frame_label(const raw_location *loc, bool base,
                       strbuilder_t *strout);
static const raw_location *prev_ruby_location(const raw_location *loc,
                                              size_t loc_len);
static int calc_lineno(const rb_iseq_t *iseq, const void *pc);
static const rb_callable_method_entry_t *
backtracie_vm_frame_method_entry(const rb_control_frame_t *cfp);

static void backtracie_frame_wrapper_mark(void *ptr);
static void backtracie_frame_wrapper_compact(void *ptr);
static void backtracie_frame_wrapper_free(void *ptr);
static size_t backtracie_frame_wrapper_memsize(const void *ptr);
static const rb_data_type_t backtracie_frame_wrapper_type = {
    .wrap_struct_name = "backtracie_frame_wrapper",
    .function = {.dmark = backtracie_frame_wrapper_mark,
                 .dfree = backtracie_frame_wrapper_free,
                 .dsize = backtracie_frame_wrapper_memsize,
#ifndef PRE_GC_MARK_MOVABLE
                 .dcompact = backtracie_frame_wrapper_compact,
#endif
                 .reserved = {0}},
    .parent = NULL,
    .data = NULL,
    // This is safe, because our free function does not do anything which could
    // yield the GVL.
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

typedef struct {
  raw_location *frames;
  size_t capa;
  int len;
} frame_wrapper_t;

static bool object_has_special_bt_handling(VALUE obj) {
  return obj == backtracie_main_object_instance || obj == rb_mRubyVMFrozenCore;
}

static bool iseq_is_block_or_eval(const rb_iseq_t *iseq) {
  if (!iseq)
    return false;
  return iseq->body->type == ISEQ_TYPE_BLOCK ||
         iseq->body->type == ISEQ_TYPE_EVAL;
}

static bool iseq_is_eval(const rb_iseq_t *iseq) {
  if (!iseq)
    return false;
  return iseq->body->type == ISEQ_TYPE_EVAL;
}

static bool class_or_module_or_iclass(VALUE obj) {
  return RB_TYPE_P(obj, T_CLASS) || RB_TYPE_P(obj, T_ICLASS) ||
         RB_TYPE_P(obj, T_MODULE);
}

bool backtracie_is_thread_alive(VALUE thread) {
  // In here we're assuming that what we got is really a Thread or its subclass.
  // This assumption NEEDS to be verified by the caller, otherwise I see a
  // segfault in your future.
  rb_thread_t *thread_pointer = (rb_thread_t *)DATA_PTR(thread);

  return !(thread_pointer->to_kill || thread_pointer->status == THREAD_KILLED);
}

void backtracie_frame_mark(const raw_location *loc) {
  rb_gc_mark(loc->iseq);
  rb_gc_mark(loc->callable_method_entry);
  rb_gc_mark(loc->self_or_self_class);
}

void backtracie_frame_mark_movable(const raw_location *loc) {
#ifdef PRE_GC_MARK_MOVABLE
  backtracie_frame_mark(loc);
#else
  rb_gc_mark_movable(loc->iseq);
  rb_gc_mark_movable(loc->callable_method_entry);
  rb_gc_mark_movable(loc->self_or_self_class);
#endif
}

void backtracie_frame_compact(raw_location *loc) {
#ifndef PRE_GC_MARK_MOVABLE
  loc->iseq = rb_gc_location(loc->iseq);
  loc->callable_method_entry = rb_gc_location(loc->callable_method_entry);
  loc->self_or_self_class = rb_gc_location(loc->self_or_self_class);
#endif
}

static int
backtracie_frame_count_for_execution_context(rb_execution_context_t *ec) {
  const rb_control_frame_t *last_cfp = ec->cfp;
  // +2 because of the two dummy frames at the bottom of the stack.
  const rb_control_frame_t *start_cfp = RUBY_VM_END_CONTROL_FRAME(ec) - 2;

  if (start_cfp < last_cfp) {
    return 0;
  } else {
    return (int)(start_cfp - last_cfp + 1);
  }
}

int backtracie_frame_count_for_thread(VALUE thread) {
  if (!backtracie_is_thread_alive(thread))
    return 0;
  rb_thread_t *thread_pointer = (rb_thread_t *)DATA_PTR(thread);

#ifndef PRE_EXECUTION_CONTEXT
  return backtracie_frame_count_for_execution_context(thread_pointer->ec);
#else
  return backtracie_frame_count_for_execution_context(thread_pointer);
#endif
}

static bool backtracie_capture_frame_for_execution_context(
    rb_execution_context_t *ec, int frame_index, raw_location *loc) {
  // The frame argument is zero-based with zero being "the frame closest to
  // where execution is now" (I couldn't decide if this was supposed to be the
  // "top" or "bottom" of the callstack; but lower frame argument --> more
  // recently called function.

  const rb_control_frame_t *cfp = ec->cfp + frame_index;
  if (!RUBY_VM_VALID_CONTROL_FRAME_P(cfp, RUBY_VM_END_CONTROL_FRAME(ec) - 1)) {
    // -1 because of the two "dummy" frames at the bottom of the stack.
    // (it's -1, not -2, because RUBY_VM_VALID_CONTROL_FRAME_P checks if the
    // given frame is >= the end frame, not >).,
    // Means we're past the end of the stack.
    BACKTRACIE_ASSERT_FAIL("called capture_frame with an invalid index");
  }

  const rb_callable_method_entry_t *cme = backtracie_vm_frame_method_entry(cfp);

  // Work out validity, or otherwise, of this frame.
  // This expression is derived from what backtrace_each in vm_backtrace.c does.
  bool is_valid = (!(cfp->iseq && !cfp->pc) &&
                   (VM_FRAME_RUBYFRAME_P(cfp) ||
                    (cme && cme->def->type == VM_METHOD_TYPE_CFUNC)));
  if (!is_valid) {
    // Don't include this frame in backtraces
    return false;
  }

  loc->is_ruby_frame = VM_FRAME_RUBYFRAME_P(cfp);
  loc->iseq = (VALUE)cfp->iseq;
  loc->callable_method_entry = (VALUE)cme;
  if (object_has_special_bt_handling(cfp->self) ||
      class_or_module_or_iclass(cfp->self)) {
    loc->self_or_self_class = cfp->self;
    loc->self_is_real_self = 1;
  } else {
    loc->self_or_self_class = rb_class_of(cfp->self);
    loc->self_is_real_self = 0;
  }
  loc->pc = cfp->pc;
  return true;
}

bool backtracie_capture_frame_for_thread(VALUE thread, int frame_index,
                                         raw_location *loc) {
  if (!backtracie_is_thread_alive(thread)) {
    return false;
  }
  rb_thread_t *thread_pointer = (rb_thread_t *)DATA_PTR(thread);

#ifndef PRE_EXECUTION_CONTEXT
  return backtracie_capture_frame_for_execution_context(thread_pointer->ec,
                                                        frame_index, loc);
#else
  return backtracie_capture_frame_for_execution_context(thread_pointer,
                                                        frame_index, loc);
#endif
}

int backtracie_frame_line_number(const raw_location *loc, size_t loc_len) {
  const raw_location *prev_rbframe = prev_ruby_location(loc, loc_len);
  if (prev_rbframe) {
    return calc_lineno((rb_iseq_t *)prev_rbframe->iseq, prev_rbframe->pc);
  } else {
    return 0;
  }
}

size_t backtracie_frame_name_cstr(const raw_location *loc, char *buf,
                                  size_t buflen) {
  strbuilder_t builder;
  strbuilder_init(&builder, buf, buflen);

  method_qualifier(loc, &builder);
  method_name(loc, &builder);

  return builder.attempted_size;
}

VALUE backtracie_frame_name_rbstr(const raw_location *loc) {
  strbuilder_t builder;
  strbuilder_init_growable(&builder, 256);

  method_qualifier(loc, &builder);
  method_name(loc, &builder);

  VALUE ret = strbuilder_to_value(&builder);
  strbuilder_free_growable(&builder);
  return ret;
}

size_t backtracie_frame_filename_cstr(const raw_location *loc, size_t loc_len,
                                      bool absolute, char *buf, size_t buflen) {
  strbuilder_t builder;
  strbuilder_init(&builder, buf, buflen);

  frame_filename(loc, loc_len, absolute, &builder);

  return builder.attempted_size;
}

VALUE backtracie_frame_filename_rbstr(const raw_location *loc, size_t loc_len,
                                      bool absolute) {
  strbuilder_t builder;
  strbuilder_init_growable(&builder, 256);

  bool fname_found = frame_filename(loc, loc_len, absolute, &builder);

  VALUE ret = Qnil;
  if (fname_found) {
    ret = strbuilder_to_value(&builder);
  }
  strbuilder_free_growable(&builder);
  return ret;
}

static bool frame_filename(const raw_location *loc, size_t loc_len,
                           bool absolute, strbuilder_t *strout) {
  const raw_location *prev_rbframe = prev_ruby_location(loc, loc_len);
  if (prev_rbframe) {
    return iseq_path((const rb_iseq_t *)prev_rbframe->iseq, absolute, strout);
  } else {
    // Couldn't find a ruby frame below loc in the location list
    return false;
  }
}

size_t backtracie_frame_label_cstr(const raw_location *loc, bool base,
                                   char *buf, size_t buflen) {
  strbuilder_t builder;
  strbuilder_init(&builder, buf, buflen);

  frame_label(loc, base, &builder);

  return builder.attempted_size;
}

VALUE backtracie_frame_label_rbstr(const raw_location *loc, bool base) {
  strbuilder_t builder;
  strbuilder_init_growable(&builder, 256);

  int label_found = frame_label(loc, base, &builder);

  VALUE ret = Qnil;
  if (label_found) {
    ret = strbuilder_to_value(&builder);
  }
  strbuilder_free_growable(&builder);
  return ret;
}

VALUE backtracie_frame_for_rb_profile(const raw_location *loc) {
  const rb_iseq_t *iseq = NULL;
  const rb_callable_method_entry_t *cme = NULL;
  if (RTEST(loc->iseq)) {
    iseq = (rb_iseq_t *)loc->iseq;
  }
  if (RTEST(loc->callable_method_entry)) {
    cme = (rb_callable_method_entry_t *)cme;
  }

  // This one is somewhat weird, but the regular MRI Ruby APIs seem to pick the
  // iseq for evals as well
  if (iseq_is_eval(iseq)) {
    return loc->iseq;
  }
  // This comes from the original rb_profile_frames logic, which would only
  // return the iseq when the cme type is not VM_METHOD_TYPE_ISEQ
  if (cme && cme->def->type != VM_METHOD_TYPE_ISEQ) {
    return loc->callable_method_entry;
  }

  if (iseq) {
    return loc->iseq;
  }
  return Qnil;
}

static int frame_label(const raw_location *loc, bool base,
                       strbuilder_t *strout) {
  if (loc->is_ruby_frame) {
    // Replicate what rb_profile_frames would do
    if (!RTEST(loc->iseq)) {
      return 0;
    }
    rb_iseq_t *iseq = (rb_iseq_t *)loc->iseq;
    VALUE label =
        base ? iseq->body->location.base_label : iseq->body->location.label;
    strbuilder_append_value(strout, label);
  } else {
    if (!RTEST(loc->callable_method_entry)) {
      return 0;
    }
    rb_callable_method_entry_t *cme =
        (rb_callable_method_entry_t *)loc->callable_method_entry;
    strbuilder_append_value(strout, rb_id2str(cme->def->original_id));
  }
  return 1;
}

static void mod_to_s_anon(VALUE klass, strbuilder_t *strout) {
  // Anonymous module/class - print the name of the first non-anonymous super.
  // something like "#{klazz.ancestors.map(&:name).compact.first}$anonymous"
  //
  // Note that if klazz is a module, we want to do this on klazz.class, not
  // klazz itself:
  //
  //   irb(main):008:0> m = Module.new
  //   => #<Module:0x00000000021a7208>
  //   irb(main):009:0> m.ancestors
  //   => [#<Module:0x00000000021a7208>]
  //   # Not very useful - nothing with a name is in the ancestor chain
  //   irb(main):010:0> m.class.ancestors
  //   => [Module, Object, Kernel, BasicObject]
  //   # Much more useful - we can call this Module$anonymous.
  //
  VALUE superclass = klass;
  VALUE superclass_name = Qnil;
  // Find an actual class - every _class_ is guaranteed to be a descendant of
  // BasicObject at least, which has a name, so we'll be able to name this
  // _something_.
  while (!RB_TYPE_P(superclass, T_CLASS)) {
    superclass = rb_class_of(superclass);
  }
  do {
    superclass = rb_class_superclass(superclass);
    BACKTRACIE_ASSERT(RTEST(superclass));
    superclass_name = rb_mod_name(superclass);
  } while (!RTEST(superclass_name));
  strbuilder_append_value(strout, superclass_name);
}

static void mod_to_s_singleton(VALUE klass, strbuilder_t *strout) {
  VALUE singleton_of = rb_class_real(klass);
  // If this is the singleton_class of a Class, or Module, we want to print
  // the _value_ of the object, and _NOT_ its class.
  // Basically:
  //    module MyModule; end;
  //    klazz = MyModule.singleton_class =>
  //        we want to output "MyModule"
  //
  //    klazz = Something.new.singleton_class =>
  //        we want to output "Something"
  //
  if (singleton_of == rb_cModule || singleton_of == rb_cClass) {
    // The first case. Use the id_attached symbol to get what this is the
    // singleton_class _of_.
    st_lookup(RCLASS_IV_TBL(klass), id__attached__, (st_data_t *)&singleton_of);
  }
  mod_to_s(singleton_of, strout);
}

static void mod_to_s_refinement(VALUE refinement_module, strbuilder_t *strout) {
  ID id_refined_class;
  CONST_ID(id_refined_class, "__refined_class__");
  VALUE refined_class = rb_attr_get(refinement_module, id_refined_class);
  ID id_defined_at;
  CONST_ID(id_defined_at, "__defined_at__");
  VALUE defined_at = rb_attr_get(refinement_module, id_defined_at);

  mod_to_s(refined_class, strout);
  strbuilder_append(strout, "$refinement@");
  mod_to_s(defined_at, strout);
}

static void mod_to_s(VALUE klass, strbuilder_t *strout) {
  if (FL_TEST(klass, FL_SINGLETON)) {
    mod_to_s_singleton(klass, strout);
    strbuilder_append(strout, "$singleton");
    return;
  }

  VALUE klass_name = rb_mod_name(klass);
  if (!RTEST(rb_mod_name(klass))) {
    mod_to_s_anon(klass, strout);
    strbuilder_append(strout, "$anonymous");
    return;
  }

  // Non-anonymous module/class.
  // something like "#{klazz.name}"
  strbuilder_append_value(strout, klass_name);
}

static void method_qualifier(const raw_location *loc, strbuilder_t *strout) {
  rb_callable_method_entry_t *cme =
      (rb_callable_method_entry_t *)loc->callable_method_entry;
  VALUE defined_class = cme ? cme->defined_class : Qnil;
  VALUE class_of_defined_class =
      RTEST(defined_class) ? rb_class_of(defined_class) : Qnil;
  VALUE self = loc->self_is_real_self ? loc->self_or_self_class : Qundef;
  VALUE self_class = loc->self_is_real_self
                         ? rb_class_of(loc->self_or_self_class)
                         : loc->self_or_self_class;
  VALUE method_target = RTEST(defined_class) ? defined_class : self_class;

  if (self == backtracie_main_object_instance) {
    strbuilder_append(strout, "Object$<main>#");
  } else if (self == rb_mRubyVMFrozenCore) {
    strbuilder_append(strout, "RubyVM::FrozenCore#");
  } else if (RTEST(class_of_defined_class) &&
             FL_TEST(class_of_defined_class, RMODULE_IS_REFINEMENT)) {
    // The method being called is defined on a refinement.
    VALUE refinement_module = class_of_defined_class;
    mod_to_s_refinement(refinement_module, strout);
    strbuilder_append(strout, "#");
  } else if (self != Qundef && class_or_module_or_iclass(self)) {
    // Means the receiver itself is a module or class, i.e. we have
    // SomeModule.foo
    mod_to_s(self, strout);
    strbuilder_append(strout, ".");
  } else {
    // Means the receiver is _not_ a module/class, so we print the name of the
    // class that the method is defined on.
    mod_to_s(method_target, strout);
    strbuilder_append(strout, "#");
  }
}

static void method_name(const raw_location *loc, strbuilder_t *strout) {
  rb_callable_method_entry_t *cme = NULL;
  rb_iseq_t *iseq = NULL;
  if (RTEST(loc->callable_method_entry)) {
    cme = (rb_callable_method_entry_t *)loc->callable_method_entry;
  }
  if (RTEST(loc->iseq)) {
    iseq = (rb_iseq_t *)loc->iseq;
  }

  if (cme) {
    // With a callable method entry, things are simple; just use that.
    VALUE method_name = rb_id2str(cme->called_id);
    strbuilder_append_value(strout, method_name);
    if (iseq_is_block_or_eval(iseq)) {
      strbuilder_append(strout, "{block}");
    }
  } else if (iseq) {
    // With no CME, we _DO NOT_ want to use iseq->base_label if we're a block,
    // because otherwise it will print something like "block in (something)". In
    // fact, using the iseq->base_label is pretty much a last resort. If we
    // manage to write _anything_ else in our backtrace, we won't use it.
    bool did_write_anything = false;
    if (loc->self_is_real_self) {
      if (RB_TYPE_P(loc->self_or_self_class, T_CLASS)) {
        // No CME, and self being a class/module, means we're executing code
        // inside a class Foo; ...; end;
        strbuilder_append(strout, "{class exec}");
        did_write_anything = true;
      }
      if (RB_TYPE_P(loc->self_or_self_class, T_MODULE)) {
        strbuilder_append(strout, "{module exec}");
        did_write_anything = true;
      }
    }
    if (iseq_is_block_or_eval(iseq)) {
      strbuilder_append(strout, "{block}");
      did_write_anything = true;
    }
    if (!did_write_anything) {
      // As a fallback, use whatever is on the base_label.
      VALUE location_name = iseq->body->location.base_label;
      strbuilder_append_value(strout, location_name);
    }
  } else {
    BACKTRACIE_ASSERT_FAIL("backtracie: don't know how to set method name");
  }
}

static const raw_location *prev_ruby_location(const raw_location *loc,
                                              size_t loc_len) {
  for (size_t i = 0; i < loc_len; i++) {
    // use_loc will == loc on the first iteration of the loop.
    // There will be exactly one loop iteration max if loc_len == 1
    const raw_location *use_loc = &loc[i];
    if (use_loc->is_ruby_frame) {
      return use_loc;
    }
  }
  // Couldn't find a ruby frame below loc in the location list
  return NULL;
}

// This is mostly a reimplementation of pathobj_path from vm_core.h
// returns true if a path was found, and false otherwise
static bool iseq_path(const rb_iseq_t *iseq, bool absolute,
                      strbuilder_t *strout) {
  if (!iseq) {
    return 0;
  }

  VALUE path_str;
#ifdef PRE_LOCATION_PATHOBJ
  rb_iseq_location_t loc = iseq->body->location;
  path_str = absolute ? loc.absolute_path : loc.path;
#else
  VALUE pathobj = iseq->body->location.pathobj;
  if (RB_TYPE_P(pathobj, T_STRING)) {
    path_str = pathobj;
  } else {
    BACKTRACIE_ASSERT(RB_TYPE_P(pathobj, T_ARRAY));
    int path_type = absolute ? PATHOBJ_REALPATH : PATHOBJ_PATH;
    path_str = RARRAY_AREF(pathobj, path_type);
  }
#endif

  if (RTEST(path_str)) {
    strbuilder_append_value(strout, path_str);
    return 1;
  } else {
    return 0;
  }
}

/**********************************************************************
  vm_backtrace.c -
  $Author: ko1 $
  created at: Sun Jun 03 00:14:20 2012
  Copyright (C) 1993-2012 Yukihiro Matsumoto
**********************************************************************/

static int calc_lineno(const rb_iseq_t *iseq, const void *pc) {
  VM_ASSERT(iseq);
  VM_ASSERT(iseq->body);
  VM_ASSERT(iseq->body->iseq_encoded);
  VM_ASSERT(iseq->body->iseq_size);
  if (!pc) {
    /* This can happen during VM bootup. */
    VM_ASSERT(iseq->body->type == ISEQ_TYPE_TOP);
    VM_ASSERT(!iseq->body->local_table);
    VM_ASSERT(!iseq->body->local_table_size);
    return 0;
  } else {
    ptrdiff_t n = (const VALUE *)pc - iseq->body->iseq_encoded;
    VM_ASSERT(n <= iseq->body->iseq_size);
    VM_ASSERT(n >= 0);
    ASSUME(n >= 0);
    size_t pos = n; /* no overflow */
    if (LIKELY(pos)) {
      /* use pos-1 because PC points next instruction at the beginning of
       * instruction */
      pos--;
    }
    return rb_iseq_line_no(iseq, pos);
  }
}

// ----------------------------------------------------------------------

/**********************************************************************

  vm_insnhelper.c - instruction helper functions.

  $Author$

  Copyright (C) 2007 Koichi Sasada

**********************************************************************/

static rb_callable_method_entry_t *copied_check_method_entry(VALUE obj,
                                                             int can_be_svar) {
  if (obj == Qfalse)
    return NULL;

#if VM_CHECK_MODE > 0
  if (!RB_TYPE_P(obj, T_IMEMO))
    rb_bug("copied_check_method_entry: unknown type: %s", rb_obj_info(obj));
#endif

  switch (imemo_type(obj)) {
  case imemo_ment:
    return (rb_callable_method_entry_t *)obj;
  case imemo_cref:
    return NULL;
  case imemo_svar:
    if (can_be_svar) {
      return copied_check_method_entry(((struct vm_svar *)obj)->cref_or_me,
                                       FALSE);
    }
  default:
#if VM_CHECK_MODE > 0
    rb_bug("copied_check_method_entry: svar should not be there:");
#endif
    return NULL;
  }
}

static const rb_callable_method_entry_t *
copied_vm_frame_method_entry(const rb_control_frame_t *cfp) {
  const VALUE *ep = cfp->ep;
  rb_callable_method_entry_t *me;

  while (!VM_ENV_LOCAL_P(ep)) {
    if ((me = copied_check_method_entry(ep[VM_ENV_DATA_INDEX_ME_CREF],
                                        FALSE)) != NULL)
      return me;
    ep = VM_ENV_PREV_EP(ep);
  }

  return copied_check_method_entry(ep[VM_ENV_DATA_INDEX_ME_CREF], TRUE);
}

// ----------------------------------------------------------------------

static const rb_callable_method_entry_t *
backtracie_vm_frame_method_entry(const rb_control_frame_t *cfp) {
#ifdef PRE_MJIT_RUBY
  // In < ruby 2.6, the symbol for rb_vm_frame_method_entry is hidden
  return copied_vm_frame_method_entry(cfp);
#else
  return rb_vm_frame_method_entry(cfp);
#endif
}

VALUE backtracie_frame_wrapper_new(size_t count) {
  frame_wrapper_t *frame_data;
  VALUE wrapper =
      TypedData_Make_Struct(backtracie_frame_wrapper_class, frame_wrapper_t,
                            &backtracie_frame_wrapper_type, frame_data);
  frame_data->capa = count;
  frame_data->len = 0;
  frame_data->frames = xcalloc(sizeof(raw_location), count);
  return wrapper;
}

raw_location *backtracie_frame_wrapper_frames(VALUE wrapper) {
  frame_wrapper_t *frame_data;
  TypedData_Get_Struct(wrapper, frame_wrapper_t, &backtracie_frame_wrapper_type,
                       frame_data);
  return frame_data->frames;
}
int *backtracie_frame_wrapper_len(VALUE wrapper) {
  frame_wrapper_t *frame_data;
  TypedData_Get_Struct(wrapper, frame_wrapper_t, &backtracie_frame_wrapper_type,
                       frame_data);
  return &frame_data->len;
}

static void backtracie_frame_wrapper_mark(void *ptr) {
  frame_wrapper_t *frame_data = (frame_wrapper_t *)ptr;
  for (int i = 0; i < frame_data->len; i++) {
    backtracie_frame_mark_movable(&frame_data->frames[i]);
  }
}
static void backtracie_frame_wrapper_compact(void *ptr) {
  frame_wrapper_t *frame_data = (frame_wrapper_t *)ptr;
  for (int i = 0; i < frame_data->len; i++) {
    backtracie_frame_compact(&frame_data->frames[i]);
  }
}
static void backtracie_frame_wrapper_free(void *ptr) {
  frame_wrapper_t *frame_data = (frame_wrapper_t *)ptr;
  xfree(frame_data->frames);
}
static size_t backtracie_frame_wrapper_memsize(const void *ptr) {
  const frame_wrapper_t *frame_data = (const frame_wrapper_t *)ptr;
  return sizeof(frame_wrapper_t) + sizeof(raw_location) * frame_data->capa;
}
