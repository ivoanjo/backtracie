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

RSpec.describe Backtracist do
  describe ".caller_locations" do
    let(:caller_locations) { described_class.caller_locations }
    let(:kernel_locations) { Kernel.caller_locations }

    it "returns an array of Backtracist::Location instances" do
      expect(caller_locations).to all(be_a(Backtracist::Location))
    end

    it "returns the same number of items as Kernel.caller_locations" do
      expect(caller_locations.size).to be kernel_locations.size
    end

    describe "each returned Backtracist::Location" do
      it "has the same absolute_path as the corresponding Kernel.caller_locations entry" do
        caller_locations.zip(kernel_locations).each do |backtracist_location, kernel_location|
          expect(backtracist_location.absolute_path).to eq kernel_location.absolute_path
        end
      end

      it "has the same base_label as the corresponding Kernel.caller_locations entry" do
        caller_locations.zip(kernel_locations).each do |backtracist_location, kernel_location|
          expect(backtracist_location.base_label).to eq kernel_location.base_label
        end
      end

      it "has the same label as the corresponding Kernel.caller_locations entry" do
        pending "The rb_profile_frame_label(...) API seems to not return the 'block (n levels) in method' that we get from other APIs"

        caller_locations.zip(kernel_locations).each do |backtracist_location, kernel_location|
          expect(backtracist_location.label).to eq kernel_location.label
        end
      end

      it "has the same lineno as the corresponding Kernel.caller_locations entry" do
        caller_locations.zip(kernel_locations).each do |backtracist_location, kernel_location|
          expect(backtracist_location.lineno).to eq kernel_location.lineno
        end
      end

      it "has the same path as the corresponding Kernel.caller_locations entry" do
        caller_locations.zip(kernel_locations).each do |backtracist_location, kernel_location|
          expect(backtracist_location.path).to eq kernel_location.path
        end
      end
    end
  end
end
