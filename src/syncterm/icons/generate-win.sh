#!/bin/sh

default_sz=$(identify -format "%w" syncterm.svg)
# The internet told me this was so...
default_density=96

if [ ! -d win32 ]
then
	mkdir win32
fi

for size in 32 36 40 48 60 64 72 80 96 100 125 150 200 256 400
do
	echo convert -background none -density $(($default_density*$size/$default_sz)) syncterm.svg win32/icon${size}.png
	convert -background none -density $(($default_density*$size/$default_sz)) syncterm.svg win32/icon${size}.png
done

default_sz=$(identify -format "%w" syncterm-mini.svg)
for size in 16 20 24 30
do
	echo convert -background none -density $(($default_density*$size/$default_sz)) syncterm-mini.svg win32/icon${size}.png
	convert -background none -density $(($default_density*$size/$default_sz)) syncterm-mini.svg win32/icon${size}.png
done
