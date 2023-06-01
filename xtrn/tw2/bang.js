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

function Closest(notsix)
{
	var j;
	var md = 2;
	var ms = -1;
	var td;

	for (j in universe) {
		if (universe[j].X === this.X && universe[j].X === this.X && universe[j].X === this.X)
			continue;
		td = this.WDistance(universe[j]);
		if (td < md) {
			if (notsix == false || universe[j].Warps.length < 6) {
				md = td;
				ms = j;
			}
		}
	}
	return ms;
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
	this.Closest=Closest;
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
		if (universe[i].Warps.length >= 6)
			break;
		if(i==j)
			continue;
		if(universe[i].WDistance(universe[j]) <= wr) {
			universe[i].Warps.push(j);
			totalwarps++;
		}
	}
}
// Fix unreachable sectors
var closest;
for (i in universe) {
	var found = false;
	for (j in universe) {
		for (k in universe[j].Warps) {
			if (universe[j].Warps[k] === i) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	if (!found) {
		closest = universe[i].Closest(true);
		if (closest !== -1) {
			universe[closest].Warps.push(i);
			totalwarps++;
		}
	}
}
// Fix unleavable sectors
for (i in universe) {
	if (universe[i].Warps.length == 0) {
		closest = universe[i].Closest(false);
		universe[i].Warps.push(closest);
		totalwarps++;
	}
}
log("Average warps per sector: "+(totalwarps/sectors));
log("Total Warps: "+totalwarps);

// Check for one-way warps
var oneways=0;
var mostwarps=0;
for(i in universe) {
	for(j in universe[i].Warps) {
		if(!universe[universe[i].Warps[j]].Warps.some(function(a,b,c) { return(a==i) } ))
			oneways++;
	}
	if (universe[i].Warps.length > mostwarps)
		mostwarps = universe[i].Warps.length;
}
log("One-way warps: "+oneways+" ("+parseInt(oneways/totalwarps*100)+"%)");
log("Most warps from a sector: "+mostwarps);
var f = new File(js.exec_dir+"/new_sector_map.js");
log("Writing to "+f.name);
if (f.open("w")) {
	f.writeln("var sector_map=[");
	for(i in universe) {
		for (j in universe[i].Warps)
			universe[i].Warps[j]++;
		f.writeln("\t{");
		f.writeln("\t\tSector:"+(parseInt(i, 10)+1)+",");
		f.writeln("\t\tWarps:["+universe[i].Warps.join(',')+"],");
		f.writeln("\t\tX:"+universe[i].X+", Y:"+universe[i].Y+", Z:"+universe[i].Z);
		f.writeln("\t},");
	}
	f.writeln('];');
	f.close();
}
else {
	log("Unable to open file!");
}
log("Done!");
