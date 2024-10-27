#!/bin/sh

default_sz=$(identify -format "%w" syncterm.svg)
# The internet told me this was so...
default_density=96

if [ ! -d syncterm.iconset ]
then
	mkdir syncterm.iconset
fi

for size in 32 64 128 256 512 1024 32x2 64x2 256x2 512x2 1024x2
do
	case $size in
	*x2)
		name=${size%x2}
		pixels=$((${size%x2}*2))
		suffix=@2x
		;;
	*)
		pixels=${size}
		name=${size}
		suffix=@1x
		;;
	esac
	echo convert -background none -density $(($default_density*$pixels/$default_sz)) syncterm.svg syncterm.iconset/icon_${name}x${name}${suffix}.png
	convert -background none -density $(($default_density*$pixels/$default_sz)) syncterm.svg syncterm.iconset/icon_${name}x${name}${suffix}.png
done

default_sz=$(identify -format "%w" syncterm-mini.svg)
pixels=16
name=16
suffix=@1x
echo convert -background none -density $(($default_density*16/$default_sz)) syncterm-mini.svg syncterm.iconset/icon_${name}x${name}${suffix}.png
convert -background none -density $(($default_density*$pixels/$default_sz)) syncterm-mini.svg syncterm.iconset/icon_${name}x${name}${suffix}.png
