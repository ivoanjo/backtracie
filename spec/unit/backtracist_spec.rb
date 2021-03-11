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

require "backtracist"

require "unit/interesting_backtrace_helper"

RSpec.describe Backtracist do
  shared_examples "an equivalent of the Ruby API (using locations)" do
    it "returns an array of Backtracist::Location instances" do
      expect(backtracist_stack).to all(be_a(Backtracist::Location))
    end

    it "returns the same number of items as the Ruby API" do
      expect(backtracist_stack.size).to be ruby_stack.size
    end

    describe "each returned Backtracist::Location" do
      it "has the same absolute_path as the corresponding Ruby API entry" do
        backtracist_stack.zip(ruby_stack).each do |backtracist_location, kernel_location|
          expect(backtracist_location.absolute_path).to eq kernel_location.absolute_path
        end
      end

      it "has the same base_label as the corresponding Ruby API entry" do
        backtracist_stack.zip(ruby_stack).each do |backtracist_location, kernel_location|
          expect(backtracist_location.base_label).to eq kernel_location.base_label
        end
      end

      it "has the same label as the corresponding Ruby API entry" do
        backtracist_stack.zip(ruby_stack).each do |backtracist_location, kernel_location|
          expect(backtracist_location.label).to eq kernel_location.label
        end
      end

      it "has the same lineno as the corresponding Ruby API entry" do
        backtracist_stack.zip(ruby_stack).each do |backtracist_location, kernel_location|
          expect(backtracist_location.lineno).to eq kernel_location.lineno
        end
      end

      it "has the same path as the corresponding Ruby API entry" do
        backtracist_stack.zip(ruby_stack).each do |backtracist_location, kernel_location|
          expect(backtracist_location.path).to eq kernel_location.path
        end
      end
    end
  end

  describe ".caller_locations" do
    context "when sampling the rspec stack" do
      let(:backtracist_stack) { described_class.caller_locations }
      let(:ruby_stack) { Kernel.caller_locations }

      it_should_behave_like "an equivalent of the Ruby API (using locations)"
    end
  end

  describe ".backtrace_locations" do
    let(:backtracist_stack) { backtraces_for_comparison.first }
    let(:ruby_stack) { backtraces_for_comparison.last }

    context "when sampling the rspec stack from another thread" do
      let!(:backtraces_for_comparison) {
        # These two procs should never be reformatted to be on different lines!
        #
        # This code is laid out like this so that the full stack traces obtained by backtracist and the regular Ruby APIs
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
    end

    context "when first argument is not a thread" do
      it do
        expect { described_class.backtrace_locations(:foo) }.to raise_exception(ArgumentError)
      end
    end

    context "when first argument is a subclass of thread" do
      let(:thread_subclass) { Class.new(Thread) }
      let(:backtracist_stack) { thread_subclass.new { described_class.backtrace_locations(Thread.current) }.value }

      it "returns an array of Backtracist::Location instances" do
        expect(backtracist_stack).to all(be_a(Backtracist::Location))
      end
    end
  end
end
