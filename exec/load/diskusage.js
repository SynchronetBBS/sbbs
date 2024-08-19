// Calculate disk usage in specified directory/pattern

"use strict";

function get(dir)
{
	var used = 0;
	var list = directory(dir);
	for(var i = 0; i < list.length; i++) {
		if(!file_isdir(list[i]))
			used += file_size(list[i]);
	}
	return used;
}

this;