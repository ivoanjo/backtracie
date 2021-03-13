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

#ifndef CFUNC_FRAMES_BACKPORT_H
#define CFUNC_FRAMES_BACKPORT_H

// Ruby 3.0 finally added support for showing "cfunc frames" (frames for methods written in C) in stack traces:
// https://github.com/ruby/ruby/pull/3299/files
//
// The diff is rather trivial, and it makes a world of difference, given how most of Ruby's core classes are written in
// C. Thus, the methods below are copied from that PR so that we can make use of this functionality on older Ruby versions.

  #ifdef CFUNC_FRAMES_BACKPORT_NEEDED
		#define backtracie_rb_profile_frame_method_name backported_rb_profile_frame_method_name

		VALUE backported_rb_profile_frame_method_name(VALUE frame);
  #else // Ruby > 3.0, just use the stock functionality
    #define backtracie_rb_profile_frame_method_name rb_profile_frame_method_name
  #endif

#endif
