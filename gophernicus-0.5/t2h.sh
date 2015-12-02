#!/bin/sh

##
## Convert text file into "C" header
##

SRC="$1"
NAME="$2"

# Check parameters
if [ ! -f "$SRC" ]; then
	echo "Usage: $0 <source> [<name>]" 1>&2
	exit 1
fi

if [ ! "$NAME" ]; then
	NAME="$SRC"
fi

# Convert file
echo "/* Automatically generated from $SRC */"
echo
echo "#define $NAME \\"
cat "$SRC" | \
	sed -n -e "1,/^*/p" | \
	grep -v "^*" | \
	sed -e "s/\\\"/\\\\\\\"/g" -e "s/^/	\"/" -e "s/$/\\\n\" \\\/"
echo "	\"\""


