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

require "backtracie/version"
require "backtracie/location"

# Note: This should be the last require, because the native extension expects all of the Ruby-defined classes above
# to exist by the time it gets initialized
require "backtracie_native_extension"

module Backtracie
  module_function

  if RUBY_VERSION < "2.5"
    def caller_locations
      # FIXME: We're having some trouble getting the current thread on older Rubies, see the FIXME on
      # backtracie_rb_profile_frames. A workaround is to just pass in the reference to the current thread explicitly
      # (and slice off a few frames, since caller_locations is supposed to start from the caller of our caller)
      backtrace_locations(Thread.current)[3..-1]
    end
  else
    def caller_locations
      Primitive.caller_locations
    end
  end

  # Defined via native code only; not redirecting via Primitive to avoid an extra stack frame on the stack
  # def backtrace_locations(thread); end

  private_class_method def ensure_object_is_thread(object)
    unless object.is_a?(Thread)
      raise ArgumentError, "Expected to receive instance of Thread or its subclass, got '#{object.inspect}'"
    end
  end
end
