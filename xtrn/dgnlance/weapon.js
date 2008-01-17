function Weapon(name, attack, power, cost)
{
	this.name=name;
	this.attack=attack;
	this.power=power;
	this.cost=cost;
}

// Read in the weapon list...
function sortweaponlist(a, b)
{
	var asum,bsum;

	asum=a.attack+a.power;
	bsum=b.attack+b.power;
	if(asum < bsum)
		return(-1);
	if(bsum < asum)
		return(1);
	return(0);
}

var weaponfile=new File(getpath()+"weapons.ini");
var weapon=new Array();

if(weaponfile.open("r", true)) {
	var all=weaponfile.iniGetSections();

	while(all.length) {
		var name=all.shift();
		var attack=weaponfile.iniGetValue(name, "Attack", 1);
		var power=weaponfile.iniGetValue(name, "Power", 1);
		var cost=weaponfile.iniGetValue(name, "Cost", 0);
		weapon.push(new Weapon(name,attack,power,cost));
	}
	weapon.sort(sortweaponlist);
}
else {
	console.attributes="HIR0";
	console.crlf();
	console.writeln("Cannot open weapons file!");
	console.crlf();
}
