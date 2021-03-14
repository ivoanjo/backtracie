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

// ruby_shards.c contains a number of borrowed functions from the MRI Ruby (usually 3.0.0) source tree:
// * A few were copy-pasted verbatim, and are dependencies for other functions
// * A few were copy-pasted and then changed so we can add features and fixes
// * A few were copy-pasted verbatim, with the objective of backporting their 3.0.0 behavior to earlier Ruby versions
// `git blame` usually documents which functions were added for what reason.

// Note that since the RUBY_MJIT_HEADER is a very special header, meant for internal use only, it has a number of quirks:
//
// 1. I've seen a few segfaults when trying to call back into original Ruby functions. E.g. even if the API is used
//    correctly, just the mere inclusion of RUBY_MJIT_HEADER causes usage to crash. Thus, as much as possible, it's
//    better to define functions OUTSIDE this file.
//
// 2. On Windows, I've observed "multiple definition of `something...'" (such as `rb_vm_ep_local_ep') whenever there
//    are multiple files in the codebase that include the RUBY_MJIT_HEADER.
//    It looks like (some?) Windows versions of Ruby define a bunch of functions in the RUBY_MJIT_HEADER itself
//    without marking them as "static" (e.g. not visible to the outside of the file), and thus the linker then complains
//    when linking together several files which all have these non-private symbols.
//    One possible hacky solution suggested by the internets is to use the "-Wl,-allow-multiple-definition" linker
//    flags to ignore this problem; instead I've chosen to implement all usage of the RUBY_MJIT_HEADER on this file --
//    no other file in backtracie shall include RUBY_MJIT_HEADER.
//    It's a simpler approach, and hopefully avoids any problems.

#include "extconf.h"
#include RUBY_MJIT_HEADER

#include "ruby_shards.h"

/**********************************************************************

  vm_backtrace.c -

  $Author: ko1 $
  created at: Sun Jun 03 00:14:20 2012

  Copyright (C) 1993-2012 Yukihiro Matsumoto

**********************************************************************/

inline static int
calc_lineno(const rb_iseq_t *iseq, const VALUE *pc)
{
    VM_ASSERT(iseq);
    VM_ASSERT(iseq->body);
    VM_ASSERT(iseq->body->iseq_encoded);
    VM_ASSERT(iseq->body->iseq_size);
    if (! pc) {
        /* This can happen during VM bootup. */
        VM_ASSERT(iseq->body->type == ISEQ_TYPE_TOP);
        VM_ASSERT(! iseq->body->local_table);
        VM_ASSERT(! iseq->body->local_table_size);
        return 0;
    }
    else {
        ptrdiff_t n = pc - iseq->body->iseq_encoded;
        VM_ASSERT(n <= iseq->body->iseq_size);
        VM_ASSERT(n >= 0);
        ASSUME(n >= 0);
        size_t pos = n; /* no overflow */
        if (LIKELY(pos)) {
            /* use pos-1 because PC points next instruction at the beginning of instruction */
            pos--;
        }
#if VMDEBUG && defined(HAVE_BUILTIN___BUILTIN_TRAP)
        else {
            /* SDR() is not possible; that causes infinite loop. */
            rb_print_backtrace();
            __builtin_trap();
        }
#endif
        return rb_iseq_line_no(iseq, pos);
    }
}

// Hacked version of Ruby's rb_profile_frames from Ruby 3.0.0 with the following changes:
// 1. Instead of just using the rb_execution_context_t for the current thread, the context is received as an argument,
//    thus allowing the sampling of any thread in the VM, not just the current one.
// 2. Also stores the cfp->iseq objects needed to extract correct labels for blocks
// 3. It correctly ignores the dummy frame at the bottom of the main Ruby thread stack, thus mimicking the behavior of
//    Ruby's backtrace_each (which is the function that is used to implement Thread#backtrace and friends)
// 4. Also stores the extra rb_vm_frame_method_entry object needed to get the correct classpath for some blocks.
static int modified_rb_profile_frames_for_execution_context(
  rb_execution_context_t *ec, // Hack #1 above
  int start,
  int limit,
  VALUE *buff,
  VALUE *correct_labels, // Hack #2 above
  VALUE *correct_blocks, // Hack #4 above
  int *lines
) {
    int i;
    const rb_control_frame_t *cfp = ec->cfp, *end_cfp = RUBY_VM_END_CONTROL_FRAME(ec);
    const rb_callable_method_entry_t *cme;

    // Hack #2 above: Here we go back one frame in addition to what the original Ruby rb_profile_frames method did.
    // Why? According to backtrace_each() in vm_backtrace.c there's two "dummy frames" (what MRI calls them) at the
    // bottom of the stack, and we need to skip them both.
    // I have no idea why the original rb_profile_frames omits this. Without this, sampling `Thread.main` always
    // returned one more frame than the regular MRI APIs (which use the aforementioned backtrace_each internally).
    end_cfp = RUBY_VM_NEXT_CONTROL_FRAME(end_cfp);

    for (i=0; i<limit && cfp != end_cfp;) {
        if (VM_FRAME_RUBYFRAME_P(cfp)) {
            if (start > 0) {
                start--;
                continue;
            }

            // Hack #3 above: Stash the iseq so we can use it for the label.
            // Otherwise ./spec/unit/backtracie_spec.rb:53 used to fail with
            //   expected: "block (3 levels) in fetch_or_store"
            //   got: "fetch_or_store"
            // ...but works with this hack.
            correct_labels[i] = (VALUE) cfp->iseq;

            // Correctly initialize these
            correct_blocks[i] = Qnil;

            /* record frame info */
            cme = rb_vm_frame_method_entry(cfp);
            if (cme && cme->def->type == VM_METHOD_TYPE_ISEQ) {
                buff[i] = (VALUE)cme;
            }
            else {
                // Hack #4 above: For some blocks, this object contains the correct full classpath that we
                // cannot otherwise get.
                // FIXME: Having yet another array for this is quite inefficient :(
                if (cme && cme->def->type == VM_METHOD_TYPE_BMETHOD) {
                  correct_blocks[i] = (VALUE)cme;
                }

                buff[i] = (VALUE)cfp->iseq;
            }

            if (lines) lines[i] = calc_lineno(cfp->iseq, cfp->pc);

            i++;
        }
        else {
            cme = rb_vm_frame_method_entry(cfp);
            if (cme && cme->def->type == VM_METHOD_TYPE_CFUNC) {
                buff[i] = (VALUE)cme;
                if (lines) lines[i] = 0;
                i++;
            }
        }

        cfp = RUBY_VM_PREVIOUS_CONTROL_FRAME(cfp);
    }

    return i;
}

int modified_rb_profile_frames(int start, int limit, VALUE *buff, VALUE *correct_labels, VALUE *correct_blocks, int *lines) {
  return modified_rb_profile_frames_for_execution_context(GET_EC(), start, limit, buff, correct_labels, correct_blocks, lines);
}

int modified_rb_profile_frames_for_thread(VALUE thread, int start, int limit, VALUE *buff, VALUE *correct_labels, VALUE *correct_blocks, int *lines) {
  // In here we're assuming that what we got is really a Thread or its subclass. This assumption NEEDS to be verified by
  // the caller, otherwise I see a segfault in your future.
  rb_thread_t *thread_pointer = (rb_thread_t*) DATA_PTR(thread);

  if (thread_pointer->to_kill || thread_pointer->status == THREAD_KILLED) return Qnil;

  return modified_rb_profile_frames_for_execution_context(thread_pointer->ec, start, limit, buff, correct_labels, correct_blocks, lines);
}

// For more details on the objective of this backport, see the comments on ruby_shards.h
// This is used for Ruby < 3.0.0
#ifdef CFUNC_FRAMES_BACKPORT_NEEDED

static const rb_callable_method_entry_t *
cframe(VALUE frame)
{
    if (frame == Qnil) return NULL;

    if (RB_TYPE_P(frame, T_IMEMO)) {
        switch (imemo_type(frame)) {
          case imemo_ment:
            {
                const rb_callable_method_entry_t *cme = (rb_callable_method_entry_t *)frame;
                switch (cme->def->type) {
                  case VM_METHOD_TYPE_CFUNC:
                    return cme;
                  default:
                    return NULL;
                }
            }
          default:
            return NULL;
        }
    }

    return NULL;
}

static VALUE
id2str(ID id)
{
    VALUE str = rb_id2str(id);
    if (!str) return Qnil;
    return str;
}
#define rb_id2str(id) id2str(id)

static const rb_iseq_t *
frame2iseq(VALUE frame)
{
    if (frame == Qnil) return NULL;

    if (RB_TYPE_P(frame, T_IMEMO)) {
        switch (imemo_type(frame)) {
          case imemo_iseq:
            return (const rb_iseq_t *)frame;
          case imemo_ment:
            {
                const rb_callable_method_entry_t *cme = (rb_callable_method_entry_t *)frame;
                switch (cme->def->type) {
                  case VM_METHOD_TYPE_ISEQ:
                    return cme->def->body.iseq.iseqptr;
                  default:
                    return NULL;
                }
            }
          default:
            break;
        }
    }
    rb_bug("frame2iseq: unreachable");
}

VALUE
backported_rb_profile_frame_method_name(VALUE frame)
{
    const rb_callable_method_entry_t *cme = cframe(frame);
    if (cme) {
        ID mid = cme->def->original_id;
        return id2str(mid);
    }
    const rb_iseq_t *iseq = frame2iseq(frame);
    return iseq ? rb_iseq_method_name(iseq) : Qnil;
}

#endif
