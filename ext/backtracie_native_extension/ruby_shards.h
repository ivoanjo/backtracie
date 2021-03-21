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

#ifndef RUBY_SHARDS_H
#define RUBY_SHARDS_H

// -----------------------------------------------------------------------------

/**********************************************************************
  method.h -
  created at: Wed Jul 15 20:02:33 2009
  Copyright (C) 2009 Koichi Sasada
**********************************************************************/

  #ifndef RUBY_MJIT_HEADER_INCLUDED
    typedef enum {
      VM_METHOD_TYPE_ISEQ,      /*!< Ruby method */
      VM_METHOD_TYPE_CFUNC,     /*!< C method */
      VM_METHOD_TYPE_ATTRSET,   /*!< attr_writer or attr_accessor */
      VM_METHOD_TYPE_IVAR,      /*!< attr_reader or attr_accessor */
      VM_METHOD_TYPE_BMETHOD,
      VM_METHOD_TYPE_ZSUPER,
      VM_METHOD_TYPE_ALIAS,
      VM_METHOD_TYPE_UNDEF,
      VM_METHOD_TYPE_NOTIMPLEMENTED,
      VM_METHOD_TYPE_OPTIMIZED, /*!< Kernel#send, Proc#call, etc */
      VM_METHOD_TYPE_MISSING,   /*!< wrapper for method_missing(id) */
      VM_METHOD_TYPE_REFINED,   /*!< refinement */
    } rb_method_type_t;
    #define VM_METHOD_TYPE_MINIMUM_BITS 4

    // Needed for Ruby 2.6 as this is not defined on any public header
    void rb_hash_bulk_insert(long, const VALUE *, VALUE);
  #endif

// -----------------------------------------------------------------------------

typedef struct {
  unsigned int is_ruby_frame : 1; // 1 -> ruby frame / 0 -> cfunc frame

  // for ruby frames where the callable_method_entry is not of type VM_METHOD_TYPE_ISEQ, most of the metadata we
  // want can be found by querying the iseq, and there may not even be an callable_method_entry
  unsigned int should_use_iseq : 1;
  unsigned int should_use_cfunc_name : 1;

  rb_method_type_t vm_method_type : VM_METHOD_TYPE_MINIMUM_BITS;
  int line_number;
  VALUE iseq;
  VALUE callable_method_entry;
  VALUE self;
  VALUE cfunc_name;
} raw_location;

  #ifndef PRE_MJIT_RUBY
    int backtracie_rb_profile_frames(int limit, raw_location *raw_locations);
    int backtracie_rb_profile_frames_for_thread(VALUE thread, int limit, raw_location *raw_locations);
  #endif
VALUE backtracie_called_id(raw_location *the_location);
VALUE backtracie_defined_class(raw_location *the_location);
VALUE backtracie_rb_vm_top_self();
bool backtracie_iseq_is_block(raw_location *the_location);
bool backtracie_iseq_is_eval(raw_location *the_location);

  #ifdef PRE_MJIT_RUBY
    int backtracie_profile_frames_from_ruby_locations(VALUE ruby_locations_array, raw_location *raw_locations);
  #endif

// -----------------------------------------------------------------------------

  // Ruby 3.0 finally added support for showing "cfunc frames" (frames for methods written in C) in stack traces:
  // https://github.com/ruby/ruby/pull/3299/files
  //
  // The diff is rather trivial, and it makes a world of difference, given how most of Ruby's core classes are written in C.
  // Thus, the methods below are copied from that PR so that we can make use of this functionality on older Ruby versions.
  #ifdef CFUNC_FRAMES_BACKPORT_NEEDED
    #define backtracie_rb_profile_frame_method_name backported_rb_profile_frame_method_name

    VALUE backported_rb_profile_frame_method_name(VALUE frame);
  #else // Ruby > 3.0, just use the stock functionality
    #define backtracie_rb_profile_frame_method_name rb_profile_frame_method_name
  #endif

#endif
