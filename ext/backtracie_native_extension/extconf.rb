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

if %w[jruby truffleruby].include?(RUBY_ENGINE)
  raise \
    "\n#{'-' * 80}\nSorry! This gem is unsupported on #{RUBY_ENGINE}. Since it relies on a lot of guts of MRI Ruby, " \
    "it's impossible to make a direct port.\n" \
    "Perhaps a #{RUBY_ENGINE} equivalent could be created -- help is welcome! :)\n#{'-' * 80}"
end

require 'mkmf'

# This warning gets really annoying when we include the Ruby mjit header file,
# let's omit it
$CFLAGS << ' ' << '-Wno-unused-function'

# Really dislike the "define everything at the beginning of the function" thing, sorry!
$CFLAGS << ' ' << '-Wno-declaration-after-statement'

# If we forget to include a Ruby header, the function call may still appear to work, but then
# cause a segfault later. Let's ensure that never happens.
$CFLAGS << ' ' << '-Werror-implicit-function-declaration'

# Enable us to use """modern""" C. Note that Ruby requires GNU extensions; specifically,
# NSIG is expected to be defined in signal.h.
$CFLAGS << ' ' << '-std=gnu99' if RUBY_VERSION < '2.4'

$CFLAGS << ' ' << '-DPRE_RB_ISEQ_TYPE' if RUBY_VERSION < '3.2'

$CFLAGS << ' ' << '-DPRE_GC_MARK_MOVABLE' if RUBY_VERSION < '2.7'

# Older Rubies don't have the MJIT header, see below for details
$defs << '-DPRE_MJIT_RUBY' if RUBY_VERSION < '2.6'

if RUBY_VERSION < '2.5'
  $CFLAGS << ' ' << '-DPRE_EXECUTION_CONTEXT' # Flag that there's no execution context, we need to use threads instead
  $CFLAGS << ' ' << '-DPRE_LOCATION_PATHOBJ'
  $CFLAGS << ' ' << '-Wno-attributes' # Silence a few warnings that we can't do anything about
end

if RUBY_VERSION < '2.4'
  $CFLAGS << ' ' << '-DPRE_VM_ENV_RENAMES' # Flag that it's a really old Ruby, and a few constants were since renamed
end

$CFLAGS << ' ' << '-DBACKTRACIE_EXPORTS'
append_cflags ['-fvisibility=hidden']
create_header

if RUBY_VERSION < '2.6'
  # Use the debase-ruby_core_source gem to get access to Ruby internal structures (no MJIT header -- the preferred
  # option -- is available for these older Rubies)

  require 'debase/ruby_core_source'
  dir_config('ruby') # allow user to pass in non-standard core include directory
  unless Debase::RubyCoreSource.create_makefile_with_core(
    proc { ['vm_core.h', 'method.h', 'iseq.h', 'regenc.h'].map { |it| have_header(it) }.uniq == [true] },
    'backtracie_native_extension'
  )
    raise 'Error during native gem setup -- `Debase::RubyCoreSource.create_makefile_with_core` failed'
  end

else
  # Use MJIT header to get access to Ruby internal structures.

  # The Ruby MJIT header is always (afaik?) suffixed with the exact RUBY version,
  # including patch (e.g. 2.7.2). Thus, we add to the header file a definition
  # containing the exact file, so that it can be used in a #include in the C code.
  header_contents =
    File.read($extconf_h)
        .sub('#endif',
             <<~EXTCONF_H.strip
               #define RUBY_MJIT_HEADER "rb_mjit_min_header-#{RUBY_VERSION}.h"

               #endif
             EXTCONF_H
            )
  File.open($extconf_h, 'w') { |file| file.puts(header_contents) }

  create_makefile 'backtracie_native_extension'
end
