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

module Backtracie
  # A more advanced version of Ruby's built-in Thread::Backtrace::Location
  class Location
    attr_accessor :absolute_path
    attr_accessor :base_label
    attr_accessor :label
    attr_accessor :lineno
    attr_accessor :path

    # Note: The order of arguments is hardcoded in the native extension in the `new_location` function --
    # keep them in sync
    def initialize(absolute_path, base_label, label, lineno, path, debug)
      @absolute_path = absolute_path
      @base_label = base_label
      @label = label
      @lineno = lineno
      @path = path
      @debug = debug

      freeze
    end

    def to_s
      if @lineno != 0
        "#{@path}:#{@lineno}:in `#{@label}'"
      else
        "#{@path}:in `#{@label}'"
      end
    end
  end
end
