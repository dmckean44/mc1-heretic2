#!/bin/sh
# Writes the module table for modules.c. Each argument is SYMS_FILE:MODULE_NAME, where the
# symbols file holds "original renamed" pairs as passed to objcopy --redefine-syms.

echo '#include "platform.h"'
echo

for arg in "$@"; do
	syms=${arg%%:*}
	grep -v ' [^ ]*__\(_Z\|__\)' "$syms" | awk '{ print "extern char " $2 "[];" }'
done

echo

i=0
for arg in "$@"; do
	syms=${arg%%:*}
	echo "static const h2_symbol_t symbols$i[] ="
	echo "{"
	grep -v ' [^ ]*__\(_Z\|__\)' "$syms" | awk '{ print "\t{ \"" $1 "\", " $2 " }," }'
	echo "	{ 0, 0 }"
	echo "};"
	echo
	i=$((i + 1))
done

echo "const h2_module_t h2_modules[] ="
echo "{"
i=0
for arg in "$@"; do
	echo "	{ \"${arg#*:}\", symbols$i },"
	i=$((i + 1))
done
echo "	{ 0, 0 }"
echo "};"
