#!/bin/sh

default_sz=$(identify -format "%w" syncterm.svg)
# The internet told me this was so...
default_density=96

if [ ! -d unix ]
then
	mkdir unix
fi

for size in 32 36 48 64 256
do
	echo convert -background none -density $(($default_density*$size/$default_sz)) syncterm.svg unix/syncterm${size}.png
	convert -background none -density $(($default_density*$size/$default_sz)) syncterm.svg unix/syncterm${size}.png
done

default_sz=$(identify -format "%w" syncterm-mini.svg)
for size in 16 22 24
do
	echo convert -background none -density $(($default_density*$size/$default_sz)) syncterm-mini.svg unix/syncterm${size}.png
	convert -background none -density $(($default_density*$size/$default_sz)) syncterm-mini.svg unix/syncterm${size}.png
done
