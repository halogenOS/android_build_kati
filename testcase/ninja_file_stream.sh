#!/bin/sh
#
# SPDX-FileCopyrightText: The halogenOS Project
# SPDX-License-Identifier: Apache-2.0

set -e

log=stderr_log
mk="$@"

# Only Kati has $(KATI_file_stream).
case "${mk}" in
  *ckati*) ;;
  *)
    echo "PASS"
    exit 0
    ;;
esac

old=200001010000

# The second line comes from the environment, so changing it makes Kati
# regenerate. KEEP is read only to make Kati regenerate with the same lines.
cat <<'EOF' > Makefile
$(KATI_file_stream out.jsonl,{"a":1,"b":2})
$(KATI_file_stream out.jsonl, $(SECOND) )
$(KATI_file_stream other.jsonl,x$(KEEP))
all: ;
EOF

expect() {
  printf '{"a":1,"b":2}\n %s \n' "$1" > expected
  if ! cmp -s out.jsonl expected; then
    echo "$2: out.jsonl differs:"
    cat out.jsonl
  fi
  if ls *.tmp >/dev/null 2>&1; then
    echo "$2: temporary file left behind"
  fi
}

# The first line of a run starts the file empty.
echo "old content" > out.jsonl
export SECOND=two
${mk} 2>${log}
expect two "first run"
if [ "$(cat other.jsonl)" != "x" ]; then
  echo "other.jsonl differs: $(cat other.jsonl)"
fi

# Without regeneration the file is not written again.
touch -t ${old} out.jsonl
${mk} 2>${log}
if grep -q regenerating ${log}; then
  echo "regenerated without a change"
fi
if [ -n "$(find out.jsonl -newer Makefile)" ]; then
  echo "out.jsonl written without regeneration"
fi
expect two "no regeneration"

# Regenerating with the same lines leaves the mtime of the file unchanged.
export KEEP=
${mk} 2>${log}
if ! grep -q regenerating ${log}; then
  echo "not regenerated after KEEP was set"
fi
if [ -n "$(find out.jsonl -newer Makefile)" ]; then
  echo "out.jsonl replaced with the same content"
fi
expect two "same lines"

# Changed lines replace the file.
export SECOND=three
${mk} 2>${log}
expect three "changed lines"
if [ -z "$(find out.jsonl -newer Makefile)" ]; then
  echo "out.jsonl not replaced with new content"
fi

# A missing file makes Kati regenerate and write it again.
rm out.jsonl
${mk} 2>${log}
if ! grep -q "out.jsonl is missing, regenerating" ${log}; then
  echo "not regenerated after out.jsonl was removed"
fi
expect three "missing file"

# A run that stops with an error leaves the file as it was.
cat <<'EOF' > Makefile
$(KATI_file_stream out.jsonl,partial)
$(error stop)
EOF
if ${mk} 2>${log}; then
  echo "error did not stop Kati"
fi
expect three "failed run"

# The function needs the evaluation of the makefile.
cat <<'EOF' > Makefile
all:
	echo $(KATI_file_stream out.jsonl,x)
EOF
if ${mk} 2>${log}; then
  echo "accepted in a rule"
elif ! grep -q "is not supported in rules" ${log}; then
  echo "in a rule: failed for another reason"
fi
cat <<'EOF' > Makefile
$(KATI_file_stream missing/out.jsonl,x)
all: ;
EOF
if ${mk} 2>${log}; then
  echo "accepted a path in a missing directory"
elif ! grep -q "fopen missing/out.jsonl.tmp" ${log}; then
  echo "a path in a missing directory: failed for another reason"
fi
expect three "rejected calls"

echo "PASS"
