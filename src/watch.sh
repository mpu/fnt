#!/bin/sh
# See LICENSE for legal matters.

set -e

if test -z $1
then
	echo "usage: watch.sh FONTPATH" 1>&2
	exit 1
fi

echo "[+] Watching $1"

while inotifywait -qr -o /dev/null -e modify "$1"
do
	echo "    reload ..."
	killall -HUP view 2>/dev/null
done
