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

#ifndef RUBY_3_0_0_H
#define RUBY_3_0_0_H

int modified_rb_profile_frames(int start, int limit, VALUE *buff, VALUE *correct_labels, int *lines);
int modified_rb_profile_frames_for_thread(VALUE thread, int start, int limit, VALUE *buff, VALUE *correct_labels, int *lines);

#endif
