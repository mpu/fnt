#!/bin/sh
echo "function Unicode() {"
echo "	a = [];"
sed -e 's/\([0-9A-F]*\);\([^;]*\);.*/	a[0x\1] = "\2";/' UnicodeData.txt
echo "	return a;"
echo "}"
