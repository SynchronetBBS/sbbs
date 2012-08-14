/* $Id$ */

/* JavaScript Random Terrain Generator
   by MCMLXXIX (2012)

USAGE:

var <variable> = generateLand( 
	<width>, 
	<height>, 
	<base>, 
	<hills>, 
	<min_radius>, 
	<max_radius>, 
	<scale> 
);

width = the width of the map array
height = the height of the map array
base = the minimum elevation of the map
hills = the number of hills to generate
max_radius = the largest any single hill can be
min_radius = the smallest any single hill can be
scale = the scale of the map (max elevation relative to base)

EXAMPLE:

var land = generateLand(20,10,0,9,10,1,10);

23333344678963322111
33344444677763332111
33433444677763332111
33333444556654332110
34444544455654333222
44445544445554333221
44445444444433201111
44444434444333100111
34443333444331100011
33333333332110000000

*/

Math.sq = function(num) {
	return num*num;
}

/* generate a land object of specified width, height, base elevation */
function generateLand(width,height,base,scale,hills,min_radius,max_radius) { 
	var map = [];
	
	if(width == undefined || height == undefined)
		throw("generateLand() requires, at minimum, width and height arguments");
	if(base == undefined)
		base = 0;
	if(scale == undefined)
		scale = 9;
	if(hills == undefined)
		hills = 20;
	if(min_radius == undefined)
		min_radius = 1;
	if(max_radius == undefined)
		max_radius = 10;
	
	/* populate matrix with base elevation values */
	for(var x=0;x<width;x++) { 
		map[x] = []; 
		for(var y=0;y<height;y++) { 
			map[x][y] = 0; 
		} 
	}
	
	/* generate specified number of "hills" */
	for(var h=0;h<hills;h++) {
		
		/* pick a random starting position */
		var x = random(width);
		var y = random(height);
		
		/* pick a hill radius between min and max */
		var radius = random(max_radius-min_radius)+min_radius;
		
		/* generate hill */
		generateFeature(map,x,y,radius);
	}
	
	/* normalize elevation values to fit between base elevation and scale */
	normalize(map,base,scale);
	
	return map; 
}

/* generate a randomized feature (elevation int_change) on a land_object starting 
from an optionally specified start_position */
function generateFeature(map,xoff,yoff,radius) {
	for(var y=-radius;y<radius;y++) {
		var halfRow=Math.sqrt(Math.sq(radius)-Math.sq(y));
		for(var x=-halfRow;x<halfRow;x++) {     
			//var h = Math.sq(radius) - (Math.sq(x) + Math.sq(y));
			var h = 1;
			var xpos = Math.floor(x);
			if(map[xoff+xpos] && map[xoff+xpos][yoff+y] >= 0)
				map[xoff+xpos][yoff+y] += h;
			if(map[xoff-xpos] && map[xoff-xpos][yoff+y] >= 0)
				map[xoff-xpos][yoff+y] += h;
		}
	}
}

/* normalize land within an acceptable range */
function normalize(map,base,scale) {
	var min;
	var max;
	
	/* iterate map and find the min and max values */
	for(var x=0;x<map.length;x++) {
		for(var y=0;y<map[x].length;y++) {
			if(!min || map[x][y] < min)
				min = map[x][y];
			if(!max || map[x][y] > max)
				max = map[x][y];
		}
	}
	
	/* iterate map again and adjust values to fit scale */
	var range = max - min;
	for(var x=0;x<map.length;x++) {
		for(var y=0;y<map[x].length;y++) {
			var n = map[x][y]-min;
			map[x][y] = ((n/range) * scale) + base;
		}
	}
}
