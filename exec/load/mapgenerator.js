/* $Id: mapgenerator.js,v 1.14 2015/03/17 15:28:52 mcmlxxix Exp $ */

/* JavaScript Random Terrain Generator
   by MCMLXXIX (2012)

USAGE:

var map = new Map(width,height);

width = the width of the map array
height = the height of the map array

PROPERTIES:

Map.min = the lowest elevation on the map
Map.max = the highest elevation on the map
Map.mean = the average elevation on the map
Map.range = the difference between the max and min elevations

SETTINGS:

Map.base = the minimum elevation of the map
Map.peak = the scale of the map (max elevation relative to base)
Map.maxRadius = the largest any single hill can be
Map.minRadius = the smallest any single hill can be
Map.hills = number of times to "poke" the map (generate elevation features)

MODES:

Map.island = [true|false] cluster features towards the center of the map
Map.lake = [true|false] invert feature changes (lower elevation instead of increase elevation)
Map.border = [true|false] add a ring around the edge of a feature

METHODS:

Map.xSection(y,width,height)
--y is the vertical offset of the map slice to create (a side profile view of the map at a given line)
--width and height are the dimensions of the matrix returned

Map.ySection(x,width,height)
--x is the horizontal offset of the map slice to create (a side profile view of the map at a given line)
--width and height are the dimensions of the matrix returned

Map.randomize(base,peak,hill)
--if arguments are not specified, Map will use default values

Map.poke(xoff,yoff,direction)
--xoff, yoff are the position on the map to change elevation (optional)
--direction is the direction in which to change the elevation (optional)
NOTE: Map.minRadius and maxRadius affect the outcome of the "poke"

Map.normalize(base,peak)
--base is the lowest acceptable elevation value (optional)
--peak is the highest acceptable elevation value (optional)

EXAMPLE:

var m = new Map(20,10);
m.randomize(0,10,100);

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

function Map(width,height) {

	/* protected map properties */
	var properties = {
		min:undefined,
		max:undefined,
		mean:undefined,
		range:undefined,
		width:0,
		height:0
	};
	
	/* map generation modes */
	var modes = {
		island:(1<<0),
		lake:(1<<1),
		border:(1<<2)
	};
	
	/* map generation settings */
	var settings = {
		minRadius:10,
		maxRadius:20,
		sectionHeight:15,
		base:0,
		peak:1,
		hills:100,
		mode:0
	};
	
	this.__defineGetter__("width",function() {
		return properties.width;
	});
	this.__defineGetter__("height",function() {
		return properties.height;
	});
	this.__defineGetter__("min",function() {
		if(isNaN(properties.min))
			setRange(this);
		return properties.min;
	});
	this.__defineGetter__("max",function() {
		if(isNaN(properties.max))
			setRange(this);
		return properties.max;
	});
	this.__defineGetter__("mean",function() {
		if(isNaN(properties.mean))
			setRange(this);
		return properties.mean;
	});
	this.__defineGetter__("range",function() {
		if(isNaN(properties.range))
			setRange(this);
		return properties.range;
	});
	this.__defineGetter__("minRadius",function() {
		return settings.minRadius;
	});
	this.__defineGetter__("maxRadius",function() {
		return settings.maxRadius;
	});
	this.__defineSetter__("minRadius",function(r) {
		if(isNaN(r) || r > settings.maxRadius)
			return false;
		settings.minRadius =  Number(r);
		return true;
	});
	this.__defineGetter__("sectionHeight",function() {
		return settings.sectionHeight;
	});
	this.__defineSetter__("sectionHeight",function(height) {
		if(isNaN(height) || height < 1 || height > 50)
			return false;
		settings.sectionHeight =  Number(height);
		return true;
	});
	this.__defineSetter__("maxRadius",function(r) {
		if(isNaN(r) || r < settings.minRadius)
			return false;
		settings.maxRadius = Number(r);
		return true;
	});
	this.__defineGetter__("island",function() {
		return settings.mode&modes.island;
	});
	this.__defineSetter__("island",function(bool) {
		if(bool) {
			if(settings.mode&modes.island)
				return false;
			settings.mode|=modes.island;
		}
		else {
			if(!(settings.mode&modes.island))
				return false;
			settings.mode&=~modes.island;
		}
	});
	this.__defineGetter__("lake",function() {
		return settings.mode&modes.lake;
	});
	this.__defineSetter__("lake",function(bool) {
		if(bool) {
			if(settings.mode&modes.lake)
				return false;
			settings.mode|=modes.lake;
		}
		else {
			if(!(settings.mode&modes.lake))
				return false;
			settings.mode&=~modes.lake;
		}
	});
	this.__defineGetter__("border",function() {
		return settings.mode&modes.lake;
	});
	this.__defineSetter__("border",function(bool) {
		if(bool) {
			if(settings.mode&modes.border)
				return false;
			settings.mode|=modes.border;
		}
		else {
			if(!(settings.mode&modes.border))
				return false;
			settings.mode&=~modes.border;
		}
	});
	this.__defineGetter__("hills",function() {
		return settings.hills;
	});
	this.__defineSetter__("hills",function(hills) {
		if(isNaN(hills))
			return false;
		settings.hills = Number(hills);
		return true;
	});
	this.__defineGetter__("base",function() {
		return settings.base;
	});
	this.__defineSetter__("base",function(base) {
		if(isNaN(base))
			return false;
		settings.base = Number(base);
		return true;
	});
	this.__defineGetter__("peak",function() {
		return settings.peak;
	});
	this.__defineSetter__("peak",function(peak) {
		if(isNaN(peak))
			return false;
		settings.peak = Number(peak);
		return true;
	});
	
	/* return a cross section (horizontal) */
	this.xSection = function(y,ceiling) {
		var section = [];
		if(!ceiling)
			ceiling = this.sectionHeight;
		for(var x=0;x<this.length;x++) {
			section[x]=[];
			
			// var a = this[x][y];
			// if(a == undefined)
				// throw("Invalid elevation data '" + a + "' at " + x + ":" + y);
			// var b = a-this.min;
			// var c = b/this.range;
			// var d = c*(height);
			var d = this[x][y];
			var i=0;
			for(;i<d;i++) 
				section[x].unshift(1);
			for(;i<ceiling;i++)
				section[x].unshift(0);
		}
		
		return section;
	}
	
	/* return a cross section (vertical) */
	this.ySection = function(x,ceiling) {
		var section = [];
		if(!ceiling)
			ceiling = this.sectionHeight;
		for(var y=0;y<this[x].length;y++) {
			section[y]=[];
			
			// var a = this[x][y];
			// if(a == undefined)
				// throw("Invalid elevation data '" + a + "' at " + x + ":" + y);
			// var b = a-this.min;
			// var c = b/this.range;
			// var d = c*(height-2);
			var d = this[x][y];
			var i=0;
			for(;i<d;i++) 
				section[y].unshift(1);
			for(;i<ceiling;i++)
				section[y].unshift(0);
		}
		
		return section.reverse();
	}
	
	/* generate a land object of specified width, height, base elevation */
	this.randomize = function(base,peak,hills) { 

		if(!hills)
			hills = settings.hills;
		
		/* generate specified number of "hills" */
		for(var h=0;h<hills;h++) 
			this.poke();
		
		/* normalize elevation values to fit between base elevation and scale */
		this.normalize(base,peak);
		
		return true;
	}
	
	/* wipe map clear */
	this.clear = function() {
		for(var x=0;x<properties.width;x++) {
			for(var y=0;y<properties.height;y++) {
				this[x][y]=0;
			}
		}
	}
	
	/* normalize elevation data between a base and peak value */
	this.normalize = function(base,peak) {
		if(base == undefined)
			base = settings.base;
		if(peak == undefined)
			peak = settings.peak;

		/* iterate map again and adjust values to fit scale */
		if(this.range == 0)
			return false;
			
		for(var x=0;x<this.length;x++) {
			for(var y=0;y<this[x].length;y++) {
				var a = this[x][y];
				if(a == undefined)
					throw("Invalid elevation data '" + a + "' at " + x + ":" + y);
				/* current elevation minus map minimum elevation */
				var b = a-this.min;
				/* distance above min elevation over elevation range */
				var c = b/this.range;
				/* adjusted current elevation based on maximum elevation */
				var d = c * (peak-base);
				/* adjusted distance above min elevation */
				var e = d + base;
				this[x][y] = e;
			}
		}
		
		setRange(this);
		return true;
	}

	/* generate a randomized feature on a map starting 
	from an optionally specified x,y position in a given direction */
	this.poke = function(xoff,yoff,radius,direction) {
	
		/* pick a hill radius between min and max */
		if(radius == undefined)
			radius = random(settings.maxRadius-settings.minRadius)+settings.minRadius;
			
		/* pick a random starting position */
		if(yoff == undefined) {
			if(settings.mode&modes.island)
				xoff = random(this.length);
			else
				xoff = random(this.length+(2*radius))-radius;
		}
		if(yoff == undefined) {
			if(settings.mode&modes.island)
				yoff = random(this[0].length);
			else
				yoff = random(this[0].length+(2*radius))-radius;
		}
			
		/* default elevation change */
		if(direction == undefined) {
			if(settings.mode&modes.lake)
				direction = -1;
			else
				direction = 1;
		}
		
		for(var y=-radius;y<radius;y++) {
			var halfRow=Math.round(Math.sqrt(Math.sq(radius)-Math.sq(y)));
			for(var x=-halfRow;x<halfRow;x++) {     
				//var h = Math.sq(radius) - (Math.sq(x) + Math.sq(y));
				if(this[xoff+x] && this[xoff+x][yoff+y] !== undefined)
					this[xoff+x][yoff+y] += direction;
				if(settings.mode&modes.border) {
					if(this[xoff-x] && this[xoff-x][yoff+y] !== undefined)
						this[xoff-x][yoff+y] += direction;
				}
			}
		}
		return true;
	}
	
	/* initialize map */
	this.init = function(width,height) {
		if(isNaN(width) || isNaN(height))
			throw("Map() width and height arguments must be integers > 0");
		if(this.length > 0) 
			this.splice(0,this.length);
		for(var x=0;x<width;x++) {
			this.push([]);
			for(var y=0;y<height;y++) {
				this[x][y]=0;
			}
		}
		
		properties.width = width;
		properties.height = height;
	}
	
	/* private range finding function */
	function setRange(map) {
		var min=undefined;
		var max=undefined;
		var mean=undefined;
		var total=0;
		
		/* iterate map and find the min and max values */
		for(var x=0;x<map.length;x++) {
			for(var y=0;y<map[x].length;y++) {
				if(min==undefined || map[x][y] < min)
					min = map[x][y];
				if(max==undefined || map[x][y] > max)
					max = map[x][y];
				total+=map[x][y];
			}
		}	
		
		mean = total / (map[0].length * map.length);
		range = max - min;
		
		//log(format("min:%f,max:%f,mean:%f,range:%f",min,max,mean,range));
		
		properties.min = min;
		properties.max = max;
		properties.mean = mean;
		properties.range = range;
	}

	/* initialize map object */
	this.init(width,height);
}
Map.prototype = new Array();	
