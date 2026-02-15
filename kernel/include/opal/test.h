#ifndef OPAL_TEST_H
#define OPAL_TEST_H

#ifdef TEST
#define STATIC_OR_TEST
#else
#define STATIC_OR_TEST static
#endif

#endif
