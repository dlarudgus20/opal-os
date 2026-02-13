#ifndef LIBKC_STRING_H
#define LIBKC_STRING_H

int str_eq(const char *a, const char *b);
int str_starts_with(const char *s, const char *prefix);
const char *skip_spaces(const char *s);

#endif
