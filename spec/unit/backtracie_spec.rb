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

require "backtracie"

require "unit/interesting_backtrace_helper"

# Used for testing below
TOP_LEVEL_BLOCK = proc { |&block| block.call }

RSpec.describe Backtracie do
  shared_examples "an equivalent of the Ruby API (using locations)" do
    it "returns an array of Backtracie::Location instances" do
      expect(backtracie_stack).to all(be_a(Backtracie::Location))
    end

    it "returns the same number of items as the Ruby API" do
      expect(backtracie_stack.size).to be ruby_stack.size
    end

    describe "each returned Backtracie::Location" do
      it "has the same absolute_path as the corresponding Ruby API entry" do
        backtracie_stack.zip(ruby_stack).each do |backtracie_location, kernel_location|
          expect(backtracie_location.absolute_path).to eq kernel_location.absolute_path
        end
      end

      it "has the same base_label as the corresponding Ruby API entry" do
        backtracie_stack.zip(ruby_stack).each do |backtracie_location, kernel_location|
          expect(backtracie_location.base_label).to eq kernel_location.base_label
        end
      end

      it "has the same label as the corresponding Ruby API entry" do
        backtracie_stack.zip(ruby_stack).each do |backtracie_location, kernel_location|
          expect(backtracie_location.label).to eq kernel_location.label
        end
      end

      it "has the same lineno as the corresponding Ruby API entry" do
        backtracie_stack.zip(ruby_stack).each do |backtracie_location, kernel_location|
          expect(backtracie_location.lineno).to eq kernel_location.lineno
        end
      end

      it "has the same path as the corresponding Ruby API entry" do
        backtracie_stack.zip(ruby_stack).each do |backtracie_location, kernel_location|
          expect(backtracie_location.path).to eq kernel_location.path
        end
      end
    end
  end

  describe ".caller_locations" do
    context "when sampling the rspec stack" do
      let(:backtracie_stack) { described_class.caller_locations }
      let(:ruby_stack) { Kernel.caller_locations }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"
    end
  end

  describe ".backtrace_locations" do
    let(:backtracie_stack) { backtraces_for_comparison.first }
    let(:ruby_stack) { backtraces_for_comparison.last }

    context "when sampling the rspec stack from another thread" do
      let!(:backtraces_for_comparison) {
        # These two procs should never be reformatted to be on different lines!
        #
        # This code is laid out like this so that the full stack traces obtained by backtracie and the regular Ruby APIs
        # match exactly. Initially I had used two lets: `let(:backtrace_locations) { Thread.new(...).value }` and
        # `let(:thread_locations) { Thread.new(...).value }` but because they were in separate lines, the resulting
        # stack traces differed -- because they pointed at two different lines!
        #
        # Hence this weird trick of declaring the two procs on the same line, so that the stack traces are similar, even
        # though the code paths followed are different.
        # I solemnly promise to only use my evil powers for good, or something.
        [proc { described_class.backtrace_locations(Thread.main) }, proc { Thread.main.backtrace_locations }]
          .map { |target_proc| Thread.new(&target_proc).value }
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"
    end

    context "when sampling an eval" do
      let!(:backtraces_for_comparison) {
        # These two function calls should never be reformatted to be on different lines!
        # See above for a note on why this looks weird
        [eval("described_class.backtrace_locations(Thread.current)"), eval("Thread.current.backtrace_locations")]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[1].qualified_method_name).to eq self.class.name + "$singleton\#{block}"
      end
    end

    context "when sampling the interesting backtrace helper" do
      let!(:backtraces_for_comparison) {
        # These two function calls should never be reformatted to be on different lines!
        # See above for a note on why this looks weird
        [sample_interesting_backtrace { described_class.backtrace_locations(Thread.current) }, sample_interesting_backtrace { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      describe "qualified_method_name behavior", :"on ruby 2.6 and above" do
        let(:expected_stack) {
          [
            "ClassA#hello{block}",
            "Kernel#loop",
            "ClassA#hello",
            "ModuleB::ClassB#hello",
            "ModuleC.hello",
            "ClassWithStaticMethod.hello",
            "ModuleD#hello",
            "Object$<main>\#{block}",
            "Object$<main>\#{block}",
            "ClassD$singleton#hello",
            "ClassE#hello",
            "UnboundMethod#bind_call",
            "ClassG$refinement@ContainsRefinement::RefinesClassG#hello",
            "ModuleE.hello",
            "ClassH#method_missing",
            "ClassF#hello{block}",
            "Integer#times",
            "ClassF#hello",
            "ClassI#hello",
            "Class$singleton.hello",
            "Object$anonymous#hello",
            "Module$anonymous.hello",
            "Object#method_with_complex_parameters",
            "ClassJ#hello{block}",
            "ClassJ#hello_helper",
            "ClassJ#hello{block}",
            "ClassJ#hello_helper",
            "ClassJ#hello",
            "ClassK\#{block}",
            "Kernel#eval",
            "ClassK#hello",
            "ClassK\#{block}",
            "BasicObject#instance_eval",
            "ClassL#hello",
            "ClassL\#{block}",
            "Kernel#eval",
            "ClassM#hello",
            "Object#top_level_hello",
            "Object$<main>\#{block}",
            "Kernel#eval",
            "Object$<main>\#{block}",
            "Integer#times",
            "Object$<main>\#{block}",
            "Integer#times",
            "Object$<main>\#{block}"
          ]
        }

        it "captures the correct class and method names" do
          pending "Work in progress"

          backtracie_stack[2..-1].zip(expected_stack).each do |backtracie_location, expected_stack_entry|
            expect(backtracie_location.qualified_method_name).to eq(expected_stack_entry)
          end
        end
      end
    end

    context "when sampling a method defined using define_method" do
      class ClassWithMethodDefinedUsingDefinedMethod
        define_method(:test_method) do |&block|
          block.call
        end
      end

      let(:test_object) { ClassWithMethodDefinedUsingDefinedMethod.new }

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [test_object.test_method { described_class.backtrace_locations(Thread.current) }, test_object.test_method { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it "includes the class name in the qualified_method_name of a test_method call frame", :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to start_with(ClassWithMethodDefinedUsingDefinedMethod.name)
      end

      it "includes the method name in the qualified_method_name of a test_method call frame", :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to end_with("test_method")
      end
    end

    context "when sampling a top-level block" do
      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [TOP_LEVEL_BLOCK.call { described_class.backtrace_locations(Thread.current) }, TOP_LEVEL_BLOCK.call { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "Object$<main>\#{block}"
      end
    end

    context "when sampling a singleton object" do
      let(:singleton_object) {
        Object.new.tap { |it|
          def it.test_method
            yield
          end
        }
      }

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [singleton_object.test_method { described_class.backtrace_locations(Thread.current) }, singleton_object.test_method { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "Object$singleton#test_method"
      end

      context "when singleton class has been expanded" do
        before do
          # While conceptually every Ruby object has a singleton class, this singleton class is actually lazily created,
          # so it's important to test both the unexpanded as well as the expanded states for it... I think
          singleton_object.singleton_class
        end

        it nil, :"on ruby 2.6 and above" do
          expect(backtracie_stack[2].qualified_method_name).to eq "Object$singleton#test_method"
        end
      end
    end

    context "when sampling a block inside a method" do
      class ClassWithMethod
        def test_method(&block)
          2.times do
            return block.call
          end
        end
      end

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [ClassWithMethod.new.test_method { described_class.backtrace_locations(Thread.current) }, ClassWithMethod.new.test_method { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "ClassWithMethod#test_method{block}"
      end
    end

    context "when sampling a module_function on a module" do
      module ModuleWithFunction
        module_function

        def test_function
          yield
        end
      end

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [ModuleWithFunction.test_function { described_class.backtrace_locations(Thread.current) }, ModuleWithFunction.test_function { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "ModuleWithFunction.test_function"
      end
    end

    context "when sampling a method directly defined on a class object" do
      class ClassWithMethodDirectlyDefined
        def self.test_method
          yield
        end
      end

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [ClassWithMethodDirectlyDefined.test_method { described_class.backtrace_locations(Thread.current) }, ClassWithMethodDirectlyDefined.test_method { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "ClassWithMethodDirectlyDefined.test_method"
      end
    end

    context "when sampling a map from an enumerable" do
      class ClassWithMethodUsingEnumerable
        def test_method(&block)
          2.times.map { block.call }.first
        end
      end

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [ClassWithMethodUsingEnumerable.new.test_method { described_class.backtrace_locations(Thread.current) }, ClassWithMethodUsingEnumerable.new.test_method { Thread.current.backtrace_locations }]
      }

      it "returns the same number of items as the Ruby API" do
        pending "Unexpectedly, we're getting one more frame than the regular Ruby API. Needs investigation" unless RUBY_VERSION < "2.6"

        expect(backtracie_stack.size).to be ruby_stack.size
      end
    end

    context "when first argument is not a thread" do
      it do
        expect { described_class.backtrace_locations(:foo) }.to raise_exception(ArgumentError)
      end
    end

    context "when sampling a refinement" do
      module TheRefinement
        refine Integer do
          def test_method
            yield
          end
        end
      end

      using TheRefinement

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [0.test_method { described_class.backtrace_locations(Thread.current) }, 0.test_method { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "Integer$refinement@TheRefinement#test_method"
      end
    end

    context "when sampling a block inside a refinement" do # I clearly have nothing better to do today
      module TheRefinementWithABlock
        refine Integer do
          def test_method
            [nil].map { yield }.first
          end
        end
      end

      using TheRefinementWithABlock

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [0.test_method { described_class.backtrace_locations(Thread.current) }, 0.test_method { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "Integer$refinement@TheRefinementWithABlock#test_method{block}"
      end
    end

    context "when sampling a method defined in an object's singleton class" do
      let(:the_singleton_class) {
        Object.new.singleton_class.tap { |it|
          def it.test_method
            yield
          end
        }
      }

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [the_singleton_class.test_method { described_class.backtrace_locations(Thread.current) }, the_singleton_class.test_method { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "Class$singleton.test_method"
      end
    end

    context "when a sampling an instance of an anonymous subclass of some class" do
      let(:object) {
        c1 = Class.new(Array)
        c2 = Class.new(c1) do
          def test_method
            yield
          end
        end
        c2.new
      }

      # These two function calls should never be reformatted to be on different lines!
      # See above for a note on why this looks weird
      let!(:backtraces_for_comparison) {
        [object.test_method { described_class.backtrace_locations(Thread.current) }, object.test_method { Thread.current.backtrace_locations }]
      }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"

      it nil, :"on ruby 2.6 and above" do
        expect(backtracie_stack[2].qualified_method_name).to eq "Array$anonymous#test_method"
      end
    end

    context "when first argument is a subclass of thread" do
      let(:thread_subclass) { Class.new(Thread) }
      let(:backtracie_stack) { thread_subclass.new { described_class.backtrace_locations(Thread.current) }.value }

      it "returns an array of Backtracie::Location instances" do
        expect(backtracie_stack).to all(be_a(Backtracie::Location))
      end
    end
  end
end
