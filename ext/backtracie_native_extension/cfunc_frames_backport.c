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

#include "extconf.h"
#include RUBY_MJIT_HEADER

#include "cfunc_frames_backport.h"

/**********************************************************************

  vm_backtrace.c -

  $Author: ko1 $
  created at: Sun Jun 03 00:14:20 2012

  Copyright (C) 1993-2012 Yukihiro Matsumoto

**********************************************************************/

// For more details on the objective of this backport, see the comments on cfunc_frames_backport.h

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
