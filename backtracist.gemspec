# frozen_string_literal: true

# backtracist: Ruby gem for beautiful backtraces
# Copyright (C) 2021 Ivo Anjo <ivo@ivoanjo.me>
#
# This file is part of backtracist.
#
# WarmBlanket is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# WarmBlanket is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with WarmBlanket.  If not, see <http://www.gnu.org/licenses/>.

lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "backtracist/version"

Gem::Specification.new do |spec|
  spec.name = "backtracist"
  spec.version = Backtracist::VERSION
  spec.authors = ["Ivo Anjo"]
  spec.email = ["ivo@ivoanjo.me"]

  spec.summary = "Ruby gem for beautiful backtraces"
  spec.description = "Ruby gem for beautiful backtraces"
  spec.homepage = "https://github.com/ivoanjo/backtracist"
  spec.license = "LGPL-3.0+"
  spec.required_ruby_version = ">= 3.0.0"

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files = Dir.chdir(File.expand_path(__dir__)) do
    `git ls-files -z`.split("\x0").reject { |f| f.match(%r{\A(?:test|spec|features)/}) }
  end
  spec.require_paths = ["lib", "ext"]
end
