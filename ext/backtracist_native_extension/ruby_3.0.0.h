#ifndef RUBY_3_0_0_H
#define RUBY_3_0_0_H

int modified_rb_profile_frames(int start, int limit, VALUE *buff, int *lines);
int modified_rb_profile_frames_for_thread(VALUE thread, int start, int limit, VALUE *buff, int *lines);

#endif
