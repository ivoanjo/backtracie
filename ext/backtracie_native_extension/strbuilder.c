#include <ruby.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "backtracie_private.h"
#include "strbuilder.h"

void strbuilder_init(strbuilder_t *str, char *buf, size_t bufsize) {
  str->original_buf = buf;
  str->original_bufsize = bufsize;
  str->curr_ptr = str->original_buf;
  str->attempted_size = 0;
  if (str->original_bufsize > 0) {
    str->original_buf[0] = '\0';
  }
  str->growable = false;
}

void strbuilder_init_growable(strbuilder_t *str, size_t initial_bufsize) {
  str->original_bufsize = initial_bufsize;
  str->original_buf = malloc(str->original_bufsize);
  str->curr_ptr = str->original_buf;
  str->attempted_size = 0;
  if (str->original_bufsize > 0) {
    str->original_buf[0] = '\0';
  }
  str->growable = true;
}

void strbuilder_free_growable(strbuilder_t *str) {
  BACKTRACIE_ASSERT(str->growable);
  free(str->original_buf);
}

static void strbuilder_grow(strbuilder_t *str) {
  ptrdiff_t offset = str->curr_ptr - str->original_buf;
  str->original_bufsize = str->original_bufsize * 2;
  str->original_buf = realloc(str->original_buf, str->original_bufsize);
  str->curr_ptr = str->original_buf + offset;
}

void strbuilder_appendf(strbuilder_t *str, const char *fmt, ...) {
  va_list fmtargs;
  va_start(fmtargs, fmt);

retry:;
  // The size left in the buffer
  size_t max_writesize =
      str->original_bufsize - (str->curr_ptr - str->original_buf);
  // vsnprintf returns the number of bytes it _would_ have written, not
  // including the null terminator.
  size_t attempted_writesize_wo_nullterm =
      vsnprintf(str->curr_ptr, max_writesize, fmt, fmtargs);
  if (attempted_writesize_wo_nullterm >= max_writesize) {
    // Can we grow & retry?
    if (str->growable) {
      strbuilder_grow(str);
      goto retry;
    }
    // If the string (including nullterm) would have exceeded the bufsize,
    // point str->curr_ptr to one-past-the-end of the buffer.
    // This will make subsequent calls to vsnprintf() receive zero for
    // max_writesize, no further things can be appended to this buffer.
    str->curr_ptr = str->original_buf + str->original_bufsize;
  } else {
    // If there's still room in the buffer, advance the curr_ptr.
    str->curr_ptr = str->curr_ptr + attempted_writesize_wo_nullterm;
  }
  str->attempted_size += attempted_writesize_wo_nullterm;
  va_end(fmtargs);
}

void strbuilder_append(strbuilder_t *str, const char *cat) {
retry:;
  size_t max_writesize =
      str->original_bufsize - (str->curr_ptr - str->original_buf);
  size_t attempted_writesize_wo_nullterm =
      strlcat(str->curr_ptr, cat, max_writesize);
  if (attempted_writesize_wo_nullterm >= max_writesize) {
    if (str->growable) {
      strbuilder_grow(str);
      goto retry;
    }
    str->curr_ptr = str->original_buf + str->original_bufsize;
  } else {
    str->curr_ptr = str->curr_ptr + attempted_writesize_wo_nullterm;
  }
  str->attempted_size += attempted_writesize_wo_nullterm;
}

void strbuilder_append_value(strbuilder_t *str, VALUE val) {
  BACKTRACIE_ASSERT(RB_TYPE_P(val, T_STRING));

  const char *val_ptr = RSTRING_PTR(val);
  size_t val_len = RSTRING_LEN(val);

retry:;
  size_t max_writesize =
      str->original_bufsize - (str->curr_ptr - str->original_buf);
  size_t chars_to_copy = val_len;
  if (chars_to_copy + 1 > max_writesize) {
    if (str->growable) {
      strbuilder_grow(str);
      goto retry;
    }
    chars_to_copy = max_writesize - 1; // leave room for NULL terminator.
  }
  memcpy(str->curr_ptr, val_ptr, chars_to_copy);
  str->curr_ptr[chars_to_copy] = '\0';
  str->attempted_size += val_len;
  if (val_len + 1 > max_writesize) {
    str->curr_ptr = str->original_buf + str->original_bufsize;
  } else {
    str->curr_ptr += val_len;
  }

  RB_GC_GUARD(val);
}

VALUE strbuilder_to_value(strbuilder_t *str) {
  return rb_str_new2(str->original_buf);
}
