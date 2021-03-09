# frozen_string_literal: true

# backtracist: Ruby gem for beautiful backtraces
# Copyright (C) 2021 Ivo Anjo <ivo@ivoanjo.me>
#
# This file is part of backtracist.
#
# backtracist is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# backtracist is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with backtracist.  If not, see <http://www.gnu.org/licenses/>.

require "backtracist/version"
require "backtracist/location"

# Note: This should be the last require, because the native extension expects all of the Ruby-defined classes above
# to exist by the time it gets initialized
require "backtracist_native_extension"

module Backtracist
  module_function

  def caller_locations
    Primitive.caller_locations
  end

  def backtrace_locations(thread)
    raise ArgumentError, "Expected to receive instance of Thread or its subclass, got '#{thread.inspect}'" unless thread.class <= Thread

    Primitive.backtrace_locations(thread)
  end
end
