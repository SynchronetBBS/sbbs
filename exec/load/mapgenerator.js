/* $Id$ */

/* JavaScript Random Terrain Generator
   by MCMLXXIX (2012)

USAGE:

var generator = new TerrainGenerator();
var map = generator.generate( 
	<width>, 
	<height>, 
	<hills>
);

width = the width of the map array
height = the height of the map array
hills = the number of hills to generate

SETTINGS:

TerrainGenerator.base = the minimum elevation of the map
TerrainGenerator.max_radius = the largest any single hill can be
TerrainGenerator.min_radius = the smallest any single hill can be
TerrainGenerator.peak = the scale of the map (max elevation relative to base)

TerrainGenerator.island_mode = force poke center-points to be within map boundaries
								(tends to cluster terrain in the center of the map)
TerrainGenerator.lake_mode = default "poke" direction is downward instead of upward
TerrainGenerator.border_mode = make a single line half-step border around "poke"

METHODS:

TerrainGenerator.generate(width,height,hills)
--must supply width and height 
--hills determines whether the generator will generate randomized terrain for you
--if hills is not specified, a "blank slate" will be returned, which 
  can be manipulated via poke() and normalize()

TerrainGenerator.poke(map,xoff,yoff,direction)
--must supply a map (2D integer array)
--xoff, yoff are the position on the map to change elevation (optional)
--direction is the direction in which to change the elevation (optional)
--min_radius and max_radius affect the outcome of the "poke"

TerrainGenerator.normalize(map,base,scale)
--must supply a map (2D integer array)
--base is the lowest acceptable elevation value (optional)
--scale is the highest acceptable elevation value (optional)

EXAMPLE:

var generator = new TerrainGenerator();
var map = generator.generate(20,10,10);

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

function TerrainGenerator() {

	/* defaults */
	this.base = 0;
	this.peak = 9;
	this.min_radius=15;
	this.max_radius=20;
	
	this.border_mode=false;
	this.island_mode=false;
	this.lake_mode=false;
	
	/* generate a land object of specified width, height, base elevation */
	this.generate = function(width,height,hills) { 

		if(width == undefined || height == undefined)
			throw("Terrain.generate() requires, at minimum, width and height arguments");
			
		/* populate matrix with base elevation values */
		var map = getMap(width,height,this.base);
		
		if(hills > 0) {
			/* generate specified number of "hills" */
			for(var h=0;h<hills;h++) 
				map = this.poke(map);
			
			/* normalize elevation values to fit between base elevation and scale */
			map = this.normalize(map);
		}
		
		return map;
	}

	/* generate a randomized feature on a map starting 
	from an optionally specified x,y position in a given direction */
	this.poke = function(map,xoff,yoff,radius,direction) {
		/* make a copy of the original map */
		var copy = copyMap(map);
		
		/* pick a hill radius between min and max */
		if(radius == undefined)
			radius = random(this.max_radius-this.min_radius)+this.min_radius;
			
		/* pick a random starting position */
		if(yoff == undefined) {
			if(this.island_mode)
				xoff = random(copy.length);
			else
				xoff = random(copy.length+(2*radius))-radius;
		}
		if(yoff == undefined) {
			if(this.island_mode)
				yoff = random(copy[0].length);
			else
				yoff = random(copy[0].length+(2*radius))-radius;
		}
			
		/* default elevation change */
		if(direction == undefined) {
			if(this.lake_mode)
				direction = -1;
			else
				direction = 1;
		}
		
		for(var y=-radius;y<radius;y++) {
			var halfRow=Math.round(Math.sqrt(Math.sq(radius)-Math.sq(y)));
			for(var x=-halfRow;x<halfRow;x++) {     
				//var h = Math.sq(radius) - (Math.sq(x) + Math.sq(y));
				if(copy[xoff+x] && copy[xoff+x][yoff+y] !== undefined)
					copy[xoff+x][yoff+y] += direction;
				if(this.border_mode) {
					if(copy[xoff-x] && copy[xoff-x][yoff+y] !== undefined)
						copy[xoff-x][yoff+y] += direction;
				}
			}
		}
		
		return copy;
	}

	/* normalize land within a given range */
	this.normalize = function(map,base,peak) {
	
		/* make a copy of the original map */
		var copy = copyMap(map);
		var min=undefined;
		var max=undefined;
		
		if(base == undefined)
			base = this.base;
		if(peak == undefined)
			peak = this.peak;
		
		/* iterate map and find the min and max values */
		for(var x=0;x<copy.length;x++) {
			for(var y=0;y<copy[x].length;y++) {
				if(min==undefined || copy[x][y] < min)
					min = copy[x][y];
				if(max==undefined || copy[x][y] > max)
					max = copy[x][y];
			}
		}
		
		/* iterate map again and adjust values to fit scale */
		var range = Math.abs(max - min);
		if(range == 0)
			return copy;
			
		for(var x=0;x<copy.length;x++) {
			for(var y=0;y<copy[x].length;y++) {
				var a = copy[x][y]-min;
				var b = a/range;
				var c = b * peak;
				var d = c + base;
				if(d < base)
					log("invalid elevation: " + d);
				copy[x][y] = d;
			}
		}
		
		return copy;
	}
	
	/* create a 2d array of given dimensions */
	function getMap(width,height,base) {
		var array = [];
		for(var x=0;x<width;x++) { 
			array[x] = []; 
			for(var y=0;y<height;y++) 
				array[x][y] = base; 
		}
		return array;
	}
	
	/* copy a map */
	function copyMap(map) {
		var copy = [];
		for(var x=0;x<map.length;x++) {
			copy[x] = [];
			for(var y=0;y<map[x].length;y++) 
				copy[x][y] = map[x][y];
		}
		return copy;
	}
}