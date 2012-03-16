/*
 * Generates a new universe...
 *
 * Sectors are solar systems... and are located in three dimensions.
 * then scaled to the universe
 *
 */

var sectors=1000;
var universe=[];
var ports=[];
var planets=[];
var i,j;
var sol;
var warpradius=1.5;

function SectorDistance(other)
{
	return(Math.sqrt(
		Math.pow(this.X-other.X,2)+
		Math.pow(this.Y-other.Y,2)+
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
}

var sol=new CubicSector(0,0,0);

for(i=0; i<sectors; i++) {
	if(i==0) {
			universe.push(sol);
	}
	else {
		var x,y,z;

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
			universe.push(new CubicSector(x,y,z));
		} while(0);
	}
}

universe.sort(function(a,b) { return(a.Distance(sol) - b.Distance(sol)) });
var wr=Math.pow((2*2*2/sectors)/((4/3)*Math.PI),1/3)*warpradius;	// warpradius 1.5 times the average sector sphere.
var totalwarps=0;
for(i in universe) {
	universe[i].Warps=[];
	for(j in universe) {
		if(i==j)
			continue;
		if(universe[i].Distance(universe[j]) <= wr) {
			universe[i].Warps.push(j);
			totalwarps++;
		}
	}
	writeln(format("%f/%f/%f - %f",universe[i].X,universe[i].Y,universe[i].Z,universe[i].Distance(sol)));
	writeln("  Warps: "+universe[i].Warps.join(', '));
}
log("Radius="+wr);
log("Average warps per sector: "+(totalwarps/sectors));
