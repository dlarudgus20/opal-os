#ifndef OPAL_ATTRIBUTES_H
#define OPAL_ATTRIBUTES_H

#include <kc/attributes.h>

#if __has_attribute(may_alias)
#define MAY_ALIAS [[gnu::may_alias]]
#else
#error "Compiler does not support may_alias attribute"
#endif

#if __has_attribute(packed)
#define PACKED [[gnu::packed]]
#else
#error "Compiler does not support packed attribute"
#endif

#endif
