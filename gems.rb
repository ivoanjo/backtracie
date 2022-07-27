# frozen_string_literal: true

source "https://rubygems.org"

gemspec

# Development dependencies
gem "rake", "~> 13.0"
gem "rake-compiler", "~> 1.1"
gem "rspec", "~> 3.10"

# Tools
gem "pry", '>= 0.14'
gem "pry-byebug"
gem "standard", "~> 1.3" unless RUBY_VERSION < "2.5"
