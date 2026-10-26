#!/bin/sh
#
# SPDX-FileCopyrightText: The halogenOS Project
# SPDX-License-Identifier: Apache-2.0

set -e

log=stderr_log
mk="$@"

# Only Kati has $(KATI_json_quote).
case "${mk}" in
  *ckati*) ;;
  *)
    echo "PASS"
    exit 0
    ;;
esac

# Every byte that JSON escapes, a comma, a newline, spaces and characters of
# two, three and four bytes in UTF-8.
cat <<'EOF' > Makefile
TEXT := $(shell printf 'a"b\\c,d\te\r\f\b\001\037é€😀')
define NL


endef
$(file >got,$(KATI_json_quote $(TEXT)$(NL) x ))
$(file >>got,$(KATI_json_quote ))
all: ;
EOF
cat <<'EOF' > expected
"a\"b\\c,d\te\r\f\b\u0001\u001fé€😀\n x "
""
EOF
${mk} >${log} 2>&1
if ! cmp -s got expected; then
  echo "quoted text differs:"
  cat got
fi

echo "PASS"
