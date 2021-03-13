# frozen_string_literal: true

# backtracie: Ruby gem for beautiful backtraces
# Copyright (C) 2021 Ivo Anjo <ivo@ivoanjo.me>
#
# This file is part of backtracie.
#
# backtracie is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# backtracie is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with backtracie.  If not, see <http://www.gnu.org/licenses/>.

require "mkmf"

# This warning gets really annoying when we include the Ruby mjit header file,
# let's omit it
$CFLAGS << " " << "-Wno-unused-function"

# On older Rubies, we need to enable a few backports. See cfunc_frames_backport.h for details.
if RUBY_VERSION < "3"
  $defs << "-DCFUNC_FRAMES_BACKPORT_NEEDED"
end

create_header

# The Ruby MJIT header is always (afaik?) suffixed with the exact RUBY version,
# including patch (e.g. 2.7.2). Thus, we add to the header file a definition
# containing the exact file, so that it can be used in a #include in the C code.
header_contents =
  File.read($extconf_h)
    .sub("#endif",
      <<~EXTCONF_H.strip
        #define RUBY_MJIT_HEADER "rb_mjit_min_header-#{RUBY_VERSION}.h"

        #endif
      EXTCONF_H
    )
File.open($extconf_h, "w") { |file| file.puts(header_contents) }

create_makefile "backtracie_native_extension"
