/* $Id$ */

/* JavaScript Random Terrain Generator
   by MCMLXXIX (2012)

//generate initial land mass
var land = generateLand(20,10,0);

// generate first level of mountain
var foothills = generateFeature(.50,1,land,{x:10,y:5}); true;

// use the same starting point and choose a smaller percentage 
// of land to increase the elevation
var mountain = generateFeature(.30,2,land,{x:10,y:5}); true;

// same deal.. smaller percentage and bigger increase
var peak = generateFeature(.15,3,land,{x:10,y:5}); true;

00001111111330000000
00000113113333300000
00000033133633100000
00000113333633000000
00100133336666311000
00111113366666111000
00133336666666600110
00003133636661111100
00003103666611100000
00000101066600100000

generateFeature() returns an array containing coordinate objects
representing every position that was modified in the operation

Notice it adds the elevation change to the existing elevation,
but only if the starting elevation matches the elevation of the specified starting position

*/

Math.sq = function(num) {
	return num*num;
}

/* generate a land object of specified width, height, base elevation */
function generateLand(width,height,base_elevation,num_hills) { 
	var map = [];
	
	/* populate matrix with base elevation values */
	for(var x=0;x<width;x++) { 
		map[x] = []; 
		for(var y=0;y<height;y++) { 
			map[x][y] = base_elevation; 
		} 
	}
	
	/* generate specified number of "hills" */
	for(var h=0;h<num_hills;h++) {
		var x = random(width);
		var y = random(height);
		var radius = random(height)+1;
		generateFeature(map,x,y,radius);
	}
	
	/* ToDo: normalize? */
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

