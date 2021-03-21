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

# The following module is extracted from
# https://ivoanjo.me/blog/2020/07/19/better-backtraces-in-ruby-using-tracepoint/
# and is similar in intent to Backtracie (provide access to more data than what the MRI APIs expose) BUT
# BetterBacktrace is implemented using the Ruby tracepoint debug APIs and works by instrumenting every method
# call/return, and thus can never be very efficient.
#
# Nevertheless, BetterBacktrace is interesting for use during development for comparison to what Backtracie returns.
# Note that BetterBacktrace cannot be "lazy loaded" -- it needs to be always running to be able to have the stack data
# available whenever better_backtrace_locations is called.
module BetterBacktrace
  MAIN_OBJECT = TOPLEVEL_BINDING.eval("self")

  TRACEPOINT = TracePoint.new(:call, :c_call, :b_call, :return, :c_return, :b_return) do |tracepoint|
    TRACEPOINT.disable do
      if tracepoint.defined_class == TracePoint
        # do nothing
      elsif [:call, :c_call, :b_call].include?(tracepoint.event)
        Thread.current[:better_stacktrace] =
          (Thread.current[:better_stacktrace] || []) <<
          [
            tracepoint.defined_class.to_s,
            [Module, Class].include?(tracepoint.self&.class), # self_class_module_or_class?
            tracepoint.method_id,
            (RUBY_VERSION >= "2.6.0" ? tracepoint.parameters : []),
            tracepoint.path,
            tracepoint.lineno,
            tracepoint.defined_class&.singleton_class? == true, # defined_class_singleton_class?
            tracepoint.event == :b_call, # block?
            nil == tracepoint.defined_class && MAIN_OBJECT.equal?(tracepoint.self) # main_object?
            # TODO: Unclear if there's any case when nil == tracepoint.defined_class and we're not looking at the main object
          ]
      else
        Thread.current[:better_stacktrace]&.pop
      end
    end
  end

  TRACEPOINT.enable

  def self.better_backtrace_locations(thread = Thread.current)
    TRACEPOINT.disable do
      thread[:better_stacktrace].dup.reverse.map { |entry| Locations.new(*entry) }
    end
  end

  Locations = Struct.new(:defined_class, :self_class_module_or_class?, :method_id, :parameters, :path, :lineno, :defined_class_singleton_class?, :block?, :main_object?) do
    def to_s
      block_suffix = block? ? "{block}" : ""
      file_and_line = "#{path}:#{lineno}"

      if main_object?
        full_signature = "Object$singleton#<main>#{block_suffix}"
      else
        instance_method = !defined_class_singleton_class?
        is_singleton = !instance_method && !self_class_module_or_class?

        class_or_module_name =
          if instance_method && !defined_class.include?("refinement") # Regular method defined in a Class or included via a Module
            defined_class
          elsif instance_method && defined_class.include?("refinement") # Refinement
            # Extract name of refined class and module where refinement was declared, something like #<refinement:ClassG@ContainsRefinement::RefinesClassG>
            refined_class = defined_class[(defined_class.index("refinement:") + 11)..(defined_class.index("@") - 1)]
            "#{refined_class}$refinement@#{defined_class.split("@").last[0..-2]}"
          elsif !is_singleton # Method directly defined on Module or a Class object
            # Extract name of original class from inside to_s, something like #<Class:ModuleC>
            defined_class[(defined_class.index(":") + 1)..(defined_class.rindex(">") - 1)]
          else # Method that was uniquely defined on a given object
            # Extract class of original object from inside to_s, something like #<Class:#<ClassD:0x0000556b7db390f0>>
            defined_class[(defined_class.index(":#<") + 3)..(defined_class.rindex(">") - 1)].split(":").first
          end

        method_divider = instance_method || is_singleton ? "#" : "."
        singleton_suffix = is_singleton ? "$singleton" : ""

        full_signature = "#{class_or_module_name}#{singleton_suffix}#{method_divider}#{method_id}#{block_suffix}"
      end

      "#{file_and_line}:in #{full_signature}(#{render_parameters})"
    end

    def render_parameters
      parameters.map { |type, name|
        case type
        when :req
          if name
            name.to_s
          else
            "()"
          end
        when :opt
          name.to_s
        when :rest
          "*#{name}"
        when :keyreq
          "#{name}:"
        when :key
          "#{name}:"
        when :keyrest
          "**#{name}"
        when :block
          "&#{name}"
        end
      }.join(", ")
    end
  end
end
