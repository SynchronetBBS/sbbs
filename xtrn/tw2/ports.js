var DefaultPort = {
		Name:'',
		Class:'',
		Commodities:[0,0,0],
		Production:[0,0,0],
		PriceVariance:[0,0,0],
		LastUpdate:0,
		OccupiedBy:0,
		Sector:0
};

var PortProperties = [
			{
				 prop:"Name"
				,name:"Name"
				,type:"String:42"
				,def:""
			}
			,{
				 prop:"Class"
				,name:"Class"
				,type:"Integer"
				,def:""
			}
			,{
				 prop:"Commodities"
				,name:"Commodities"
				,type:"Array:3:Float"
				,def:[0,0,0]
			}
			,{
				 prop:"Production"
				,name:"Production (units/day)"
				,type:"Array:3:SignedInteger"
				,def:[0,0,0]
			}
			,{
				 prop:"PriceVariance"
				,name:"Price Variance (percentage)"
				,type:"Array:3:SignedInteger"
				,def:[0,0,0]
			}
			,{
				 prop:"LastUpdate"
				,name:"Port last produced"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"OccupiedBy"
				,name:"Occupied By"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Sector"
				,name:"Sector"
				,type:"Integer"
				,def:0
			}
		];

function SolReport()
{
	var today=system.datestr(system.datestr());
	var holdprice=parseInt(50*Math.sin(0.89756*(today/86400)) + 0.5)+500;
	var fighterprice=parseInt(10*Math.sin(0.89714*(today/86400) + 1.5707) + 0.5)+100;
	var turnprice=300;

	console.writeln("Commerce report for Sol: " + system.timestr());
	console.writeln("  Cargo holds: " + holdprice + " credits/hold");
	console.writeln("  Fighters   : " + fighterprice + " credits/fighter");
	console.writeln("  Turns      : "+turnprice+" credits each.");
	console.crlf();

	return([holdprice,fighterprice,turnprice]);
}

/*
 * Sol has no persistant data so locking isn't needed
 */
function DockAtSol()
{
	/* 33200 */
	var prices=SolReport();
	var maxbuy;
	var buy;
	
	console.attributes="HR";
	console.writeln("You have " + player.Credits + " credits.");
	console.crlf();
	/* Holds */
	maxbuy=parseInt(player.Credits/prices[0]);
	if(player.Holds+maxbuy>50)
		maxbuy=50-player.Holds;
	if(maxbuy > 0) {
		console.write("How many holds do you want to buy [0]-"+maxbuy+"? ");
		buy=InputFunc([{min:0,max:maxbuy}]);
		if(buy>0) {
			player.Holds+=buy;
			player.Credits -= buy*prices[0];
			player.Put();
			console.writeln("You have " + player.Credits + " credits.");
		}
	}
	/* Fighters */
	maxbuy=parseInt(player.Credits/prices[1]);
	if(player.Fighters+maxbuy>9999)
		maxbuy=9999-player.Fighters;
	if(maxbuy > 0) {
		console.write("How many fighters do you want to buy [0]-"+maxbuy+"? ");
		buy=InputFunc([{min:0,max:maxbuy}]);
		if(buy>0) {
			player.Fighters+=buy;
			player.Credits -= buy*prices[1];
			player.Put();
			console.writeln("You have " + player.Credits + " credits.");
		}
	}
	/* Turns */
	maxbuy=parseInt(player.Credits/prices[2]);
	if(maxbuy > 0) {
		console.write("How many turns do you want to buy [0]? ");
		buy=InputFunc([{min:0,max:maxbuy}]);
		if(buy>0) {
			player.TurnsLeft+=buy;
			player.Credits -= buy*prices[2];
			player.Put();
			console.writeln("You have " + player.Credits + " credits.");
		}
	}
}

function PortReport(portNum) {
	db.lock(Settings.DB,'ports.'+portNum,LOCK_WRITE);
	var port=db.read(Settings.DB,'ports.'+portNum);
	var i;

	if(port==null) {
		db.unlock(Settings.DB,'ports.'+portNum);
		return(null);
	}
	/* 33000 */
	LockedProduction(port);
	db.write(Settings.DB,'ports.'+portNum,port);
	db.unlock(Settings.DB,'ports.'+portNum,port);
	var ret=new Array(Commodities.length);
	for(i=0; i<Commodities.length; i++) {
		ret[i]=new Object();
		ret[i].Vary=port.PriceVariance[i];
		ret[i].Number=parseInt(port.Commodities[i]);
		ret[i].Price=parseInt(Commodities[i].price*(1-ret[i].Vary*ret[i].Number/port.Production[i]/1000)+0.5);
	}
	console.crlf();
	console.writeln("Commerce report for "+port.Name+": "+system.timestr());
	console.crlf();
	console.writeln(" Items     Status   # units  in holds");
	console.attributes="HK";
	console.writeln(" =====     ======   =======  ========");
	for(i=0; i<Commodities.length; i++) {
		console.attributes=['HR','HG','HY','HB','HM','HC','HW'][i];
		console.writeln(format("%s %-7s  %-7d  %-7d",
				 Commodities[i].disp
				,ret[i].Vary<0?"Buying":"Selling"
				,ret[i].Number
				,player.Commodities[i]));
	}
	return(ret);
}

function Transact(type, price, vary, avail)
{
	var buy=vary<0?false:true;
	var youare=buy?"buy":"sell";
	var weare=buy?"sell":"buy";
	var emptyholds=player.Holds;
	var max=0;

	var i;
	for(i=0; i<Commodities.length; i++)
		emptyholds -= player.Commodities[i];

	if(buy)
		max=emptyholds;
	else
		max=player.Commodities[type];

	if(max > parseInt(avail))
		max=parseInt(avail);

	if(max < 1)
		return(-1);

	console.crlf();
	console.writeln("You have "+player.Credits+" credits and "+emptyholds+" empty cargo holds.");
	console.crlf();
	console.writeln("We are "+weare+"ing up to "+avail+".  You have "+player.Commodities[type]+" in your holds.");

	console.write("How many holds of "+Commodities[type].name.toLowerCase()+" do you want to "+youare+" ["+max+"]? ");
	var amount=InputFunc([{min:0,max:max,def:max}]);

	if(amount>0 && amount<=max) {
		var done=false;
		console.writeln("Agreed, "+amount+" units.");
		var haggles=random(3)+2;
		var offer=0;
		for(var hag=0; hag<haggles; hag++) {
			if(hag==haggles-1)
				console.write("Our final offer is ");
			else
				console.write("We'll "+weare+" them for ");
			console.writeln(parseInt(price*amount*(1+vary/1000)+0.5));
			offer=0;
			while(offer==0) {
				console.write("Your offer? ");
				offer=InputFunc([{min:0,max:(buy?player.Credits:(price*amount*10))}]);
				if(offer==0)
					break;
				if(offer < price*amount/10 || offer > price*amount*10) {
					console.writeln("That was pretty funny, Now make a serious offer.");
					offer=0;
				}
			}
			if(offer==0)
				break;
			if(!buy && offer <= price*amount) {
				console.writeln("We'll take them!");
				player.Credits += offer;
				player.Commodities[type] -= amount;
				player.Put();
				return(amount);
			}
			if(buy && offer >= price*amount) {
				console.writeln("Sold!");
				player.Credits -= offer;
				player.Commodities[type] += amount;
				player.Put();
				return(amount);
			}
			var worstoffer=parseInt(amount*price*(1-vary/250/(hag+1)) + 0.5);
			if(buy && offer < worstoffer)
				break;
			if(!buy && offer > worstoffer)
				break;
			price = 0.7*price + 0.3*offer/amount;
		}

		if(offer > 0) {
			if(buy) {
				console.writeln("Too low.  We'll sell them to local traders.");
				return(0);
			}
			console.writeln("Too high.  We'll buy them from local traders.");
		}
		return(0);
	}
	return(0);
}

function DockAtRegularPort()
{
	var sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
	var amount;
	var sale;
	var count;
	var port;

	/* Lock the port file and ensure we can dock... */
	db.lock(Settings.DB,'ports.'+sector.Port,LOCK_WRITE);
	port=db.read(Settings.DB,'ports.'+sector.Port);
	if(port.OccupiedBy != 0) {
		console.writeln("The port is busy.  Try again later.");
		db.unlock(Settings.DB,'ports.'+sector.Port);
		return;
	}
	port.OccupiedBy=player.Record;
	LockedProduction(port);
	db.write(Settings.DB,'ports.'+sector.Port,port);
	db.unlock(Settings.DB,'ports.'+sector.Port);
	sale=PortReport(sector.Port);
	console.attributes="HR";
	count=0;

	console.writeln();

	console.attributes="HY";
	/* Sell... */
	for(i=0; i<Commodities.length; i++) {
		if(sale[i].Vary < 0) {
			amount=Transact(i, sale[i].Price, sale[i].Vary, sale[i].Number);
			if(amount >= 0) {
				count++;
				port.Commodities[i] -= amount;
				db.write(Settings.DB,'ports.'+sector.Port,port,LOCK_WRITE);
			}
		}
	}
	/* Buy... */
	for(i=0; i<Commodities.length; i++) {
		if(sale[i].Vary > 0) {
			amount=Transact(i, sale[i].Price, sale[i].Vary, sale[i].Number);
			if(amount >= 0) {
				count++;
				port.Commodities[i] -= amount;
				db.write(Settings.DB,'ports.'+sector.Port,port,LOCK_WRITE);
			}
		}
	}
	if(count==0) {
		console.attributes="IC";
		console.writeln("You don't have anything they want, and they don't have anything you can buy");
		console.attributes="C";
	}

	port.OccupiedBy=0;
	db.write(Settings.DB,'ports.'+sector.Port,port,LOCK_WRITE);
}


function DockAtPort()
{
	console.attributes="HR";
	console.crlf();
	console.writeln("Docking...");
	if(player.TurnsLeft<1) {
		console.writeln("Sorry, but you don't have any turns left.");
		return;
	}
	var sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
	if(sector.Port<1) {
		console.writeln("There are no ports in this sector.");
		return;
	}
	player.TurnsLeft--;
	player.Ported=true;
	player.Put();
	console.writeln("One turn deducted");
	if(player.Sector==1) {
		DockAtSol();
	}
	else {
		DockAtRegularPort();
	}
	player.Ported=false;
	player.Put();
}

function InitializePorts()
{
	var port;
	
	if(this.uifc) uifc.pop("Placing Ports");

	db.lock(Settings.DB,'ports',LOCK_WRITE);
	db.write(Settings.DB,'ports',[]);
	db.push(Settings.DB,'ports',DefaultPort);

	/* Place ports */
	for(i=0; i<ports_init.length; i++) {
		port=eval(DefaultPort.toSource());
		for(prop in ports_init[i]) {
			if(port[prop]!=undefined)
				port[prop]=ports_init[i][prop];
		}
		port.Production=[ports_init[i].OreProduction, ports_init[i].OrgProduction, ports_init[i].EquProduction];
		port.PriceVariance=[ports_init[i].OreDeduction,ports_init[i].OrgDeduction,ports_init[i].EquDeduction];

		db.lock(Settings.DB,'sectors.'+ports_init[i].Sector,LOCK_WRITE);
		var sector=db.read(Settings.DB,'sectors.'+ports_init[i].Sector);
		sector.Port=i+1;
		db.write(Settings.DB,'sectors.'+ports_init[i].Sector,sector);
		db.unlock(Settings.DB,'sectors.'+ports_init[i].Sector);

		db.push(Settings.DB,'ports',port);
	}
	db.unlock(Settings.DB,'ports');
}
