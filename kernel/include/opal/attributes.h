#ifndef OPAL_ATTRIBUTES_H
#define OPAL_ATTRIBUTES_H

#include <kc/attributes.h>

#if __has_attribute(may_alias)
#define MAY_ALIAS __attribute__((may_alias))
#else
#error "Compiler does not support may_alias attribute"
#endif

#endif
