#!/bin/sh

##
## Fetch system manufacturer and product name from BIOS
##

#
# This needs to be run as root and probably only works
# on Linux. This could be run automatically at boot
# time to figure out a nice description for the system.
#

DMI=/usr/sbin/dmidecode
OUT=/var/run/gophernicus-system-info

if [ "`whoami`" != "root" ]; then
	echo "$0: Must be run as root"
	exit 1
fi

if [ -x "$DMI" ]; then
	rm -f $OUT
	$DMI -s system-manufacturer | tr '\n' ' ' > $OUT
	$DMI -s system-product-name >> $OUT
fi

