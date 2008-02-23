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
		];

var ports=new RecordFile(fname("ports.dat"), PortProperties);

function DockAtPort()
{
	console.attributes="HR";
	console.crlf();
	console.writeln("Docking...");
	if(player.TurnsLeft<1) {
		console.writeln("Sorry, but you don't have any turns left.");
		return;
	}
	var sector=sectors.Get(player.Sector);
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

function DockAtSol()
{
	/* 33200 */
	var prices=SolReport();
	console.attributes="HR";
	console.writeln("You have " + player.Credits + " credits.");
	console.crlf();
	/* Holds */
	var maxbuy=parseInt(player.Credits/prices[0]);
	if(player.Holds+maxbuy>50)
		maxbuy=50-player.Holds;
	if(maxbuy > 0) {
		console.write("How many holds do you want to buy [0]? ");
		var buy=InputFunc([{min:0,max:maxbuy}]);
		if(buy>0) {
			player.Holds+=buy;
			player.Credits -= buy*prices[0];
			player.Put();
			console.writeln("You have " + player.Credits + " credits.");
		}
	}
	/* Fighters */
	var maxbuy=parseInt(player.Credits/prices[1]);
	if(player.Fighters+maxbuy>9999)
		maxbuy=9999-player.Fighters;
	if(maxbuy > 0) {
		console.write("How many fighters do you want to buy [0]? ");
		var buy=InputFunc([{min:0,max:maxbuy}]);
		if(buy>0) {
			player.Fighters+=buy;
			player.Credits -= buy*prices[1];
			player.Put();
			console.writeln("You have " + player.Credits + " credits.");
		}
	}
	/* Turns */
	var maxbuy=parseInt(player.Credits/prices[2]);
	if(maxbuy > 0) {
		console.write("How many turns do you want to buy [0]? ");
		var buy=InputFunc([{min:0,max:maxbuy}]);
		if(buy>0) {
			player.TurnsLeft+=buy;
			player.Credits -= buy*prices[2];
			player.Put();
			console.writeln("You have " + player.Credits + " credits.");
		}
	}
}

function DockAtRegularPort()
{
	var sector=sectors.Get(player.Sector);

	/* Lock the port file and ensure we can dock... */
	if(!Lock(ports.file.name, bbs.node_num, true, 5)) {
		console.writeln("The port is busy.  Try again later.");
		return;
	}
	var port=ports.Get(sector.Port);
	if(port.OccupiedBy != 0) {
		console.writeln("The port is busy.  Try again later.");
		Unlock(ports.file.name);
		return;
	}
	port.OccupiedBy=player.Record;
	port.Put();
	Unlock(ports.file.name);
	Production(port);
	var sale=PortReport(sector);
	console.attributes="HR";
	var count=0;

	console.writeln();

	console.attributes="HY";
	/* Sell... */
	for(i=0; i<Commodities.length; i++) {
		if(sale[i].Vary < 0) {
			var amount=Transact(i, sale[i].Price, sale[i].Vary, sale[i].Number);
			if(amount >= 0) {
				count++;
				port.Commodities[i] += amount;
				port.Put();
			}
		}
	}
	/* Buy... */
	for(i=0; i<Commodities.length; i++) {
		if(sale[i].Vary > 0) {
			var amount=Transact(i, sale[i].Price, sale[i].Vary, sale[i].Number);
			if(amount >= 0) {
				count++;
				port.Commodities[i] -= amount;
				port.Put();
			}
		}
	}
	if(count==0) {
		console.attributes="IC";
		console.writeln("You don't have anything they want, and they don't have anything you can buy");
		console.attributes="C";
	}

	if(!Lock(ports.file.name, bbs.node_num, true, 10))
		twmsg.writeln("!!! Error locking ports file!");
	port.OccupiedBy=0;
	port.Put();
	Unlock(ports.file.name);
}


function PortReport(sec) {
	var port=ports.Get(sec.Port);
	var i;

	if(port==null)
		return(null);
	/* 33000 */
	Production(port);
	var ret=new Array(Commodities.length);
	for(i=0; i<Commodities.length; i++) {
		ret[i]=new Object();
		ret[i].Vary=port.PriceVariance[i];
		ret[i].Number=parseInt(port.Commodities[i]);
		ret[i].Price=parseInt(Commodities[i].price*(1-ret[i].Vary*ret[i].Number/port.Production[i]/1000)+.5);
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

function SolReport()
{
	var holdprice=parseInt(50*Math.sin(.89756*(today/86400)) + .5)+500;
	var fighterprice=parseInt(10*Math.sin(.89714*(today/86400) + 1.5707) + .5)+100;
	var turnprice=300;

	console.writeln("Commerce report for Sol: " + system.timestr());
	console.writeln("  Cargo holds: " + holdprice + " credits/hold");
	console.writeln("  Fighters   : " + fighterprice + " credits/fighter");
	console.writeln("  Turns      : "+turnprice+" credits each.");
	console.crlf();

	return([holdprice,fighterprice,turnprice]);
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
			console.writeln(parseInt(price*amount*(1+vary/1000)+.5));
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
			var worstoffer=parseInt(amount*price*(1-vary/250/(hag+1)) + .5);
			if(buy && offer < worstoffer)
				break;
			if(!buy && offer > worstoffer)
				break;
			price = .7*price + .3*offer/amount;
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

function InitializePorts()
{
	uifc.pop("Placing Ports");
	port=ports.New();
	port.Put();

	/* Place ports */
	for(i=0; i<ports_init.length; i++) {
		port=ports.New();
		for(prop in ports_init[i])
			port[prop]=ports_init[i][prop];
		port.Production=[ports_init[i].OreProduction, ports_init[i].OrgProduction, ports_init[i].EquProduction];
		port.PriceVariance=[ports_init[i].OreDeduction,ports_init[i].OrgDeduction,ports_init[i].EquDeduction];

		var sector=sectors.Get(ports_init[i].Position);
		sector.Port=port.Record;
		sector.Put();
		ports_init[i].LastUpdate=system.datestr(time()-15*86400);

		port.Put();
	}
}
