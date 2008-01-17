function Armour(name, power, cost)
{
	this.name=name;
	this.cost=cost;
	this.power=power;
}

// Read in the armour list...
function sortarmourlist(a, b)
{
	if(a.power < b.power)
		return(-1);
	if(b.power < a.power)
		return(1);
	return(0);
}

var armourfile=new File(getpath()+"armours.ini");
var armour=new Array();

if(armourfile.open("r", true)) {
	var all=armourfile.iniGetSections();

	while(all.length) {
		var name=all.shift();
		var power=armourfile.iniGetValue(name, "Power", 1);
		var cost=armourfile.iniGetValue(name, "Cost", 0);
		armour.push(new Armour(name,power,cost));
	}
	armour.sort(sortarmourlist);
}
else {
	console.attributes="HIR0";
	console.crlf();
	console.writeln("Cannot open armours file!"+armourfile.name);
	console.crlf();
	console.pause();
}
