#ifndef STRBUILDER_H
#define STRBUILDER_H

#include <ruby.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char *original_buf;
  char *curr_ptr;
  size_t original_bufsize;
  size_t attempted_size;
  bool growable;
} strbuilder_t;

void strbuilder_append(strbuilder_t *str, const char *cat);
void strbuilder_appendf(strbuilder_t *str, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void strbuilder_append_value(strbuilder_t *str, VALUE val);
VALUE strbuilder_to_value(strbuilder_t *str);
void strbuilder_init(strbuilder_t *str, char *buf, size_t bufsize);
void strbuilder_init_growable(strbuilder_t *str, size_t initial_bufsize);
void strbuilder_free_growable(strbuilder_t *str);
#endif
