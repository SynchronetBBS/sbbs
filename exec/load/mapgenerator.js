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

/* generate a land object of specified width, height, base elevation */
function generateLand(width,height,base_elevation) { 
	var a = {
		map:[],
		width:width,
		height:height,
		elevation:base_elevation
	}; 
	for(var w=0;w<width;w++) { 
		a.map[w] = []; 
		for(var h=0;h<height;h++) { 
			a.map[w][h] = base_elevation; 
		} 
	} 
	return a; 
}

/* generate a randomized feature (elevation int_change) on a land_object starting 
from an optionally specified start_position */
function generateFeature(fill_percent,int_change,land_object,start_position) { 
	var totalSize = land.width*land.height; 
	var featureSize = Math.floor(fill_percent * totalSize); 
	var featureStack = [];
	var elevation = land.elevation;
	var position = {
		x:random(land.width),
		y:random(land.height)
	};	

	/* if we've been supplied a starting position, use it */
	if(start_position) {
		position = start_position;
		elevation = land_object.map[position.x][position.y];
	}
	
	/* set starting position */
	land_object.map[position.x][position.y] = elevation + int_change;
	featureStack.push(position);
	
	while(featureStack.length < featureSize) {
	
		/* generate a new random position */
		position = {
			x:random(land_object.width),
			y:random(land_object.height)
		};
		
		/* validate this position */
		if(!validatePosition(position))
			continue;
		
		/* this randomly chosen position is already of the desired feature type */
		if(land_object.map[position.x][position.y] == elevation + int_change)
			continue;
		
		var compass = getSurroundings(position,land);
		for each(var direction in compass) {
			/* a neighbor to this random position is of the desired feature */
			if(land_object.map[direction.x][direction.y] == elevation + int_change) {
				land_object.map[position.x][position.y] = elevation + int_change;
				featureStack.push(position);
				break;
			}
		}
	}

	return featureStack;
} 

/* random() bug workaround */
function validatePosition(position) {
	/* if we received invalid random coordinates */
	if(position.x < 0 || position.x >= land.width)
		return false;
	if(position.y < 0 || position.y >= land.height)
		return false;
	return true;
}

/* return all valid neighboring coordinates from a given position */
function getSurroundings(position,land) {
	var compass={};
	if(position.x > 0)
		compass.west = { x:position.x-1,y:position.y };
	if(position.y > 0)
		compass.north = { x:position.x,y:position.y-1 };
	if(position.x < land.width-1)
		compass.east = { x:position.x+1,y:position.y };
	if(position.y < land.height-1)
		compass.south = { x:position.x,y:position.y+1 };
	return compass;
}