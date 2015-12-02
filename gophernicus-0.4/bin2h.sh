#!/bin/sh

##
## Convert binary file into "C" header
##

SRC="$1"
NAME="$2"

# Check parameters
if [ ! -f "$SRC" ]; then
	echo "Usage: $0 <source>" 1>&2
	exit 1
fi

if [ ! "$NAME" ]; then
	NAME="$SRC"
fi

# Convert file
echo "/* Automatically generated from $SRC */"
echo
echo "#define $NAME { \\"

hexdump -v -e '16/1 "0x%02x, "' -e '" \\\n"' "$SRC"

echo "}"


