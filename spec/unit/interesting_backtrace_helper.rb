# The following tries to reproduce the most interesting (contrived?) call stack I can think of, with as many weird
# variants as possible.
#
# It is implemented as a separate thread that receives procs to execute and writes back their results. It on purpose
# executes at the top-level, so we can see cases where methods are called on the "main" object.

SAMPLE_REQUESTS_QUEUE = Queue.new
SAMPLE_RESPONSES_QUEUE = Queue.new

def sample_interesting_backtrace(&block)
  SAMPLE_REQUESTS_QUEUE.push(block)
  SAMPLE_RESPONSES_QUEUE.pop
end

# ----

class ClassA
  def hello
    while true
      SAMPLE_RESPONSES_QUEUE.push(SAMPLE_REQUESTS_QUEUE.pop.call)
    end
  end
end

module ModuleB
  class ClassB < ClassA
    def hello
      super
    end
  end
end

module ModuleC
  def self.hello
    ModuleB::ClassB.new.hello
  end
end

class ClassWithStaticMethod
  def self.hello
    ModuleC.hello
  end
end

module ModuleD
  def hello
    ClassWithStaticMethod.hello
  end
end

class ClassC
  include ModuleD
end

$a_proc = proc { ClassC.new.hello }

$a_lambda = lambda { $a_proc.call }

class ClassD; end

$class_d_object = ClassD.new

def $class_d_object.hello
  $a_lambda.call
end

class ClassE
  def hello
    $class_d_object.hello
  end
end

class ClassG
  def hello
    raise "This should not be called"
  end
end

module ContainsRefinement
  module RefinesClassG
    refine ClassG do
      def hello
        ClassE.instance_method(:hello).bind_call(ClassF.new)
      end
    end
  end
end

module ModuleE
  using ContainsRefinement::RefinesClassG

  def self.hello
    ClassG.new.hello
  end
end

class ClassH
  def method_missing(name, *_)
    super unless name == :hello

    ModuleE.hello
  end
end

class ClassF < ClassE
  def hello(arg1, arg2, test1, test2)
    1.times {
      ClassH.new.hello
    }
  end
end

$singleton_class = Object.new.singleton_class

def $singleton_class.hello
  ClassF.new.hello(0, 1, 2, 3)
end

$anonymous_instance = Class.new do
  def hello
    $singleton_class.hello
  end
end.new

$anonymous_module = Module.new do
  def self.hello
    $anonymous_instance.hello
  end
end

def method_with_complex_parameters(a, b = nil, *c, (d), f:, g: nil, **h, &i)
  $anonymous_module.hello
end

def top_level_hello
  method_with_complex_parameters(0, 1, 2, [3, 4], f: 5, g: 6, h: 7, &proc {})
end

SAMPLE_BACKGROUND_THREAD = Thread.new do
  1.times {
    1.times {
      eval("top_level_hello()")
    }
  }
end
