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

require "bundler/gem_tasks"
require "rspec/core/rake_task"
require "standard/rake"
require "rake/extensiontask"

Rake::ExtensionTask.new("backtracie_native_extension")

RSpec::Core::RakeTask.new(:spec) do |task|
  # This hack allows easily passing arguments to rspec, e.g. doing
  # bundle exec rake spec -- ./spec/unit/backtracie_spec.rb:123
  if ARGV.include?("--")
    arguments = ARGV[ARGV.index("--")..-1]
    puts "opts: #{arguments}"
    task.rspec_opts = arguments.join(" ")
  end
end

task default: [:compile, :spec, :'standard:fix']
