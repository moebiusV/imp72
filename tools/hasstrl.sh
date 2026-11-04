#!/bin/sh
# hasstrl.sh: -DHAVE_STRL where the C library has strlcpy and strlcat (they link and work), else nothing: stage0/strl.c then supplies them
. "$(dirname "$0")/mktmpdir.sh"; mktmpdir hasstrl
cat > $t/h.c <<'EOF'
#include <stddef.h>
size_t (strlcpy)(char *, const char *, size_t);
size_t (strlcat)(char *, const char *, size_t);
int main(void) { char b[8]; return (int)(strlcpy(b, "a", sizeof b) + strlcat(b, "b", sizeof b)) - 3; }
EOF
if ${CC:-cc} -o $t/h $t/h.c >/dev/null 2>&1 && $t/h; then echo -DHAVE_STRL; fi
