#ifndef BACKTRACIE_PRIVATE_H
#define BACKTRACIE_PRIVATE_H

#include <ruby.h>

// Need to define an assert macro - we might have just used RUBY_ASSERT, but
// that's not exported in Ruby < 2.7.
#define BACKTRACIE_ASSERT(expr) BACKTRACIE_ASSERT_MSG((expr), (#expr))
#define BACKTRACIE_ASSERT_MSG(expr, msg)                                       \
  do {                                                                         \
    if ((expr) == 0) {                                                         \
      rb_bug("backtracie gem: %s:%d: %s", __FILE__, __LINE__, msg);            \
    }                                                                          \
  } while (0)
#define BACKTRACIE_ASSERT_FAIL(msg) BACKTRACIE_ASSERT_MSG(0, msg)

bool backtracie_is_thread_alive(VALUE thread);

#endif
