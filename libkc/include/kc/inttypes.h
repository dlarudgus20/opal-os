#ifndef KC_INTTYPES_H
#define KC_INTTYPES_H

#define PRI64_PREFIX    "l"
#define PRIPTR_PREFIX   "l"

#define PRId8   "d"
#define PRId16  "d"
#define PRId32  "d"
#define PRId64  PRI64_PREFIX "d"

#define PRIi8   "i"
#define PRIi16  "i"
#define PRIi32  "i"
#define PRIi64  PRI64_PREFIX "i"

#define PRIu8   "u"
#define PRIu16  "u"
#define PRIu32  "u"
#define PRIu64  PRI64_PREFIX "u"

#define PRIx8   "x"
#define PRIx16  "x"
#define PRIx32  "x"
#define PRIx64  PRI64_PREFIX "x"

#define PRIX8   "X"
#define PRIX16  "X"
#define PRIX32  "X"
#define PRIX64  PRI64_PREFIX "X"

#define PRIdPTR PRIPTR_PREFIX "d"
#define PRIiPTR PRIPTR_PREFIX "i"
#define PRIuPTR PRIPTR_PREFIX "u"
#define PRIxPTR PRIPTR_PREFIX "x"
#define PRIXPTR PRIPTR_PREFIX "X"

#endif
