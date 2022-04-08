#!/bin/sh
#
# Resource generating script for the Printer Application Framework
#
# Copyright © 2019-2021 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#
# Usage:
#
#   pappl-makeresheader filename [ ... filename ] >filename.h
#

for file in "$@"; do
	varname=$(basename "$file" | sed -e '1,$s/[ -.]/_/g')
	echo "/* $file */"
	case $file in
		*.icc | *.jpg | *.otf | *.otc | *.png | *.ttc | *.ttf | *.woff | *.woff2)
			echo "static unsigned char $varname[] = {"
			od -t u1 -A n -v "$file" | awk '{for (i = 1; i <= NF; i ++) printf("%s,", $i); print "";}'
			echo "};"
			;;
		*)
			echo "static const char * const $varname ="
			sed -e '1,$s/\\/\\\\/g' -e '1,$s/"/\\"/g' "$file" | awk '{print "\"" $0 "\\n\""}'
			echo ";"
			;;
	esac
done
