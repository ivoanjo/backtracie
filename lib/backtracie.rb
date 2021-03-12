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

  def caller_locations
    Primitive.caller_locations
  end

  # Defined via native code only; not redirecting via Primitive to avoid an extra stack frame on the stack
  # def backtrace_locations(thread); end

  private_class_method def ensure_object_is_thread(object)
    unless object.is_a?(Thread)
      raise ArgumentError, "Expected to receive instance of Thread or its subclass, got '#{object.inspect}'"
    end
  end
end
