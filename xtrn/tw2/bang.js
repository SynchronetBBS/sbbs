/*
 * Generates a new universe...
 *
 * Sectors are solar systems... and are located in three dimensions.
 * then scaled to the universe
 *
 */

var sectors=1000;	// Total sectors in the known universe
var warpradius=1.5;	// Ratio of radius of a sphere containing the averge
			// volume of space per sector to radius of max jump
			// distance
			// Eg: if the average sector was a 10 cubic mile
			// sphere, it would have a radius of 1.34 miles.
			// The maximum warp distance would be warpradius*1.34
			// or (assuming 1.5) 2.01 miles.
			// Obviously, the units we're using are a bit bigger.
var spin=0.1;		// Distance in known universe radii the universe
			// using the same ration to average bolume as above

var wr=Math.pow(((4/3*Math.PI)/sectors)/((4/3)*Math.PI),1/3)*warpradius;	// Calculate warp radius
var so=Math.pow(1/sectors,1/3)*spin;	// Calculate spin offset
var universe=[];
var ports=[];
var planets=[];
var i,j,k;
var sol;

function SectorDistance(other)
{
	return(Math.sqrt(
		Math.pow(this.X-other.X,2)+
		Math.pow(this.Y-other.Y,2)+
		Math.pow(this.Z-other.Z,2)
	));
}

function WarpDistance(other)
{
	return(Math.sqrt(
		Math.pow(this.X-other.X,2)+
		Math.pow(this.Y-other.Y-so,2)+
		Math.pow(this.Z-other.Z,2)
	));
}

function CubicSector(x,y,z)
{
	this.X=x;				// -1 to 1
	this.Y=y;				// -1 to 1
	this.Z=z;				// -1 to 1
	this.R=Math.sqrt(			// Radius from earth (Arbitraries)
		Math.pow(this.X,2)+
		Math.pow(this.Y,2)+
		Math.pow(this.Z,2)
	);
	this.Az=Math.atan2(this.Y,this.X);	// Azimuth from coreward (Radians)
	this.Al=Math.acos(this.Z/this.R);	// Altitude from galactic plane (Radians)
	this.Distance=SectorDistance;
	this.WDistance=WarpDistance;
}

function SphereSector(R, Al, Az)
{
	this.Az=Az;
	this.Al=Al;
	this.R=R;
	this.X=this.R*Math.cos(Az)*Math.sin(Al);
	this.Y=this.R*Math.sin(Al)*Math.sin(Az);
	this.Z=this.R*Math.cos(Al);
	this.Distance=SectorDistance;
	this.WDistance=WarpDistance;
}

var sol=new CubicSector(0,0,0);

for(i=0; i<sectors; i++) {
	if(i==0) {
			universe.push(sol);
	}
	else {
		var x,y,z,n;

check:
		do {
			x=Math.random()*2-1;
			y=Math.random()*2-1;
			z=Math.random()*2-1;
			for(j in universe) {
				if(parseInt(universe[j].X*10000)==parseInt(x*10000)
						&& parseInt(universe[j].Y*10000)==parseInt(y*10000)
						&& parseInt(universe[j].Z*10000)==parseInt(z*10000)) {
					log("Duplicate location");
					continue check;
				}
				
			}
			n=new CubicSector(x,y,z);
			if(sol.Distance(n)>1)		// Ensure the universe is spherical
				continue check;
			universe.push(n);
			break;
		} while(1);
	}
}
log("len="+universe.length);

universe.sort(function(a,b) { return(a.Distance(sol) - b.Distance(sol)) });
var totalwarps=0;
for(i in universe) {
	universe[i].Warps=[];
	for(j in universe) {
		if(i==j)
			continue;
		if(universe[i].WDistance(universe[j]) <= wr) {
			universe[i].Warps.push(j);
			totalwarps++;
		}
	}
}
log("Average warps per sector: "+(totalwarps/sectors));
log("Total Warps: "+totalwarps);

// Check for one-way warps
var oneways=0;
for(i in universe) {
	for(j in universe[i].Warps) {
		if(!universe[universe[i].Warps[j]].Warps.some(function(a,b,c) { return(a==i) } ))
			oneways++;
	}
}
log("One-way warps: "+oneways+" ("+parseInt(oneways/totalwarps*100)+"%)");
