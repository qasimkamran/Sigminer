#!/bin/bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <path-to-so> <symbol-name>" >&2
  exit 1
fi

SO="$1"
SYM="$2"

if [ ! -f "$SO" ]; then
  echo "ERROR: file not found: $SO" >&2
  exit 1
fi

# Must output:
#   - .debug_info
#   - .debug_abbrev
#   - .debug_str
# Must have either:
#   - .symtab
#   - .dynsym
readelf -S "$SO" | egrep 'debug_info|debug_abbrev|debug_str|symtab|dynsym|gnu_debuglink'

# Must output a non-empty Build ID. Useful for locating split debug info.
readelf -n "$SO" | grep -A2 'Build ID'

# Optional check: dumps .gnu_debuglink if present (split-debug filename + CRC).
# No output is fine when debug info is embedded in this .so.
readelf -x .gnu_debuglink "$SO" 2>/dev/null | head

# Quick sanity check: verifies exported dynamic symbols exist.
nm -D --defined-only "$SO" | head

# Resolve target symbol address from .symtab first (then .dynsym fallback below).
ADDR=$(nm --defined-only "$SO" 2>/dev/null | awk -v s="$SYM" '$3==s{print "0x"$1; exit}')
if [ -z "$ADDR" ]; then
  # Fallback for exported-only symbols.
  ADDR=$(nm -D --defined-only "$SO" 2>/dev/null | awk -v s="$SYM" '$3==s{print "0x"$1; exit}')
fi

# Hard fail when name->address resolution failed in both symbol tables.
if [ -z "$ADDR" ]; then
  echo "ERROR: symbol '$SYM' not found in .symtab or .dynsym for $SO" >&2
  exit 1
fi

# Success check: should show a DW_TAG_subprogram covering ADDR.
echo "Resolved $SYM to $ADDR"
llvm-dwarfdump --lookup="$ADDR" "$SO" | head -80
