
/*	Atlantis v1.0  13 September 1993
	Copyright 1993 by Russell Wallace

	This program may be freely used, modified and distributed.  It may not be
	sold or used commercially without prior written permission from the author.
*/

/*
	Ported to Synchronet JavaScript by Stephen Hurd (Deuce)
*/

var script_dir='.';
try { throw barfitty.barf(barf) } catch(e) { script_dir=e.fileName }
script_dir=script_dir.replace(/[\/\\][^\/\\]*$/,'');
script_dir=backslash(script_dir);

load(script_dir+'gamedata.js');
if(js.global.scramble==undefined)
	load(script_dir+'utilfuncs.js');
if(js.global.Troop==undefined)
	load(script_dir+'troop.js');

function mistakestr(fact_no,str) {}

function mistake2(order,str) {}

function mistakeu(unit,str) {}

function sparagraph(battles,str,indent,istr) {}

function lovar (n)
{
	n = parseInt(n/2);
	return ((random(n) + 1) + (random(n) + 1));
}

function distribute (old,nw,n)
{
	var i,t;

	if(!(nw <= old))
		throw("distribute called with new ("+nw+") > old ("+old+")!");

	if (old == 0)
		return 0;

	t = parseInt(n / old) * nw;
	for (i = (n % old); i; i--)
		if (random(old) < nw)
			t++;

	return t;
}

function process_form()
{
	var r,u,S,u2,S2;
			/* FORM orders */

	log("Processing FORM orders...");

	for (r in regions)
		/* We can't do for u in ... since we will potentially be growing the units array */
		for (u=0; u<regions[r].units.length; u++)
			for (S in regions[r].units[u].orders)
				switch(regions[r].units[u].orders[S].command) {
					case K_FORM:
						u2 = new Unit(regions[r]);

						u2.alias = regions[r].units[u].orders[S].args.shift();
						if (u2.alias == 0)
							u2.alias = regions[r].units[u].orders[S].args.shift();

						u2.faction = u.faction;
						u2.building = u.building;
						u2.ship = u.ship;
						u2.behind = u.behind;
						u2.guard = u.guard;
						u2.orders = regions[r].units[u].orders[S].suborders;
						break;
				}
}

function removenofromarray(array,obj)
{
	var i;

	for(i in array) {
		if(array[i].no==obj.no)
			array.splice(i,1);
	}
}

function addnotoarray(array,obj)
{
	/* If no is already in array, remove (then re-add) */
	removenofromarray(array,obj);
	array.push(obj);
}

function removefromarray(array,no)
{
	var i;

	for(i in array) {
		if(array[i]==no)
			array.splice(i,1);
	}
}

function addtoarray(array,no)
{
	/* If no is already in array, remove (then re-add) */
	removefromarray(array,no);
	array.push(no);
}

function getmoney (r,u,n)
{
	var i,u2,unit2;

	n -= u.money;

	for (u2 in r.units) {
		unit2=r.units[u2];
		if (unit2.faction.no == u.faction.no && u2.no != u.no)
		{
			i = Math.min (unit2.money,n);
			unit2.money -= i;
			u.money += i;
			n -= i;
		}
	}
}

function maketroops (tp,u,terrain,left,infront,runeswords)
{
	var i;
	var t;
	var skills=new Array(MAXSKILLS);	// TODO: These were static!
	var items=new Array(MAXITEMS);		// TODO: These were static!

	for (i = 0; i < MAXSKILLS; i++)
		skills[i] = u.effskill (i);
	for (i = 0; i < MAXITEMS; i++)
		items[i] = u.items[i];

	left[u.side] += u.number;
	if (!u.behind)
		infront[u.side] += u.number;

	for (i = u.number; i; i--)
	{
		t=new Troop();

		t.unit = u;
		t.side = u.side;
		t.skill = -2;
		t.behind = u.behind;

		if (u.combatspell >= 0)
			t.missile = true;
		else if (items[I_RUNESWORD] && skills[SK_SWORD])
		{
			t.weapon = I_SWORD;
			t.skill = skills[SK_SWORD] + 2;
			t.runesword = true;
			items[I_RUNESWORD]--;
			runeswords[u.side]++;

			if (items[I_HORSE] && skills[SK_RIDING] >= 2 && terrain == T_PLAIN)
			{
				t.skill += 2;
				items[I_HORSE]--;
			}
		}
		else if (items[I_LONGBOW] && skills[SK_LONGBOW])
		{
			t.weapon = I_LONGBOW;
			t.missile = true;
			t.skill = skills[SK_LONGBOW];
			items[I_LONGBOW]--;
		}
		else if (items[I_CROSSBOW] && skills[SK_CROSSBOW])
		{
			t.weapon = I_CROSSBOW;
			t.missile = true;
			t.skill = skills[SK_CROSSBOW];
			items[I_CROSSBOW]--;
		}
		else if (items[I_SWORD] && skills[SK_SWORD])
		{
			t.weapon = I_SWORD;
			t.skill = skills[SK_SWORD];
			items[I_SWORD]--;

			if (items[I_HORSE] && skills[SK_RIDING] >= 2 && terrain == T_PLAIN)
			{
				t.skill += 2;
				items[I_HORSE]--;
			}
		}

		if (u.spells[SP_HEAL] || items[I_AMULET_OF_HEALING] > 0)
		{
			t.canheal = true;
			items[I_AMULET_OF_HEALING]--;
		}

		if (items[I_RING_OF_POWER])
		{
			t.power = 1;
			items[I_RING_OF_POWER]--;
		}

		if (items[I_SHIELDSTONE])
		{
			t.shieldstone = true;
			items[I_SHIELDSTONE]--;
		}

		if (items[I_CLOAK_OF_INVULNERABILITY])
		{
			t.invulnerable = true;
			items[I_CLOAK_OF_INVULNERABILITY]--;
		}
		else if (items[I_PLATE_ARMOR])
		{
			t.armor = 2;
			items[I_PLATE_ARMOR]--;
		}
		else if (items[I_CHAIN_MAIL])
		{
			t.armor = 1;
			items[I_CHAIN_MAIL]--;
		}

		if (u.building && u.building.sizeleft)
		{
			t.inside = 2;
			u.building.sizeleft--;
		}

		tp.push(t);
	}

	return tp;
}

/* Instant orders - diplomacy etc. */
function process_instant()
{
	var r,u,S,i,tmp;
	var region,unit,order;

	log ("Processing instant orders...");

	for (r in regions) {
		region=regions[r];
		for (u in region.units) {
			unit=region.units[u];
			for (S in unit.orders) {
				order=unit.orders[S];
				switch (order.command)
				{
					case -1:
						mistake2 (order,"Order not recognized");
						break;

					case K_ACCEPT:
						tmp=findfaction(order.args.shift());
						if(tmp != null)
							addtoarray(unit.faction.accept, tmp.no);
						break;

					case K_ADDRESS:
						/* TODO: Should we just remove this command? */
						tmp=order.args.shift();
						if (tmp.length==0)
						{
							mistake2 (order,"No address given");
							break;
						}
						unit.faction.addr=tmp;

						log(format("%s is changing address to %s.\n",unit.faction.name,
								  unit.faction.addr));
						break;

					case K_ADMIT:
						tmp=findfaction(order.args.shift());
						if(tmp != null)
							addtoarray(unit.faction.admin, tmp.no);
						break;

					case K_ALLY:
						tmp = findfaction(order.args.shift());

						if (tmp == null)
						{
							mistake2 (order,"Faction not found");
							break;
						}

						if (tmp.no == unit.faction.no)
							break;

						if (parseInt(order.args.shift()))
							addtoarray(unit.faction.allies, tmp.no);
						else
							removefromarray(unit.faction.allies, tmp.no);
						break;

					case K_BEHIND:
						unit.behind=new Boolean(parseInt(order.args.shift()));
						break;

					case K_COMBAT:
						if (order.args.length==0)
						{
							unit.combatspell = -1;
							break;
						}

						i = order.args.shift();

						if (i < 0 || !unit.cancast (i))
						{
							mistake2 (order,"Spell not found");
							break;
						}

						if (!spelldata[i].iscombatspell)
						{
							mistake2 (order,"Not a combat spell");
							break;
						}

						unit.combatspell = i;
						break;

					case K_DISPLAY:
						switch (order.args.shift())
						{
							case K_BUILDING:
								if (unit.building==null)
								{
									mistake2 (order,"Not in a building");
									break;
								}

								if (!unit.owner)
								{
									mistake2 (order,"Building not owned by you");
									break;
								}
								if(order.args.length > 0)
									unit.building.display=order.args.shift();
								break;

							case K_SHIP:
								if (unit.ship==null)
								{
									mistake2 (order,"Not in a ship");
									break;
								}

								if (!unit.owner)
								{
									mistake2 (order,"Ship not owned by you");
									break;
								}
								if(order.args.length > 0)
									unit.ship.display=order.args.shift();
								break;

							case K_UNIT:
								if(order.args.length > 0)
									unit.display=order.args.shift();
								break;

							default:
								mistake2 (order,"Order not recognized");
								break;
						}
						break;

					case K_GUARD:
						/* MUST UNSHIFT THE ARG IF IT'S TRUE! */
						tmp=order.args.shift();
						if (tmp)
							order.args.unshift(tmp);
						else
							unit.guard = 0;
						break;

					case K_NAME:
						if(order.args.length==0)
							break;
						switch (parseInt(order.args.shift()))
						{
							case K_BUILDING:
								if (unit.building==null)
								{
									mistake2 (order,"Not in a building");
									break;
								}

								if (!unit.owner)
								{
									mistake2 (order,"Building not owned by you");
									break;
								}
								unit.building.name=order.args.shift();
								break;

							case K_FACTION:
								unit.faction.name=order.args.shift();
								break;

							case K_SHIP:
								if (unit.ship==null)
								{
									mistake2 (order,"Not in a ship");
								}

								if (!unit.owner)
								{
									mistake2 (order,"Ship not owned by you");
									break;
								}
								unit.ship.name=order.args.shift();
								break;

							case K_UNIT:
								unit.name=order.args.shift();
								break;

							default:
								mistake2 (order,"Order not recognized");
								break;
						}
						// TODO: Previously, names could not contain ()
						break;

					case K_RESHOW:
						i = parseInt(order.args.shift());

						if (i < 0 || i >= MAXSPELLS || !unit.faction.seendata[i])
						{
							mistake2 (order,"Spell not found");
							break;
						}

						unit.faction.showdata[i] = true;
						break;
				}
			}
		}
	}
}

/* FIND orders */
function process_find()
{
	/* TODO: Remove? */
	var r,u,S,f,unit,order;

	log ("Processing FIND orders...");

	for (r in regions)
		for (u in regions[r].units) {
			unit=regions[r].units[u];
			for (S in regions[r].units[u].orders) {
				order=regions[r].units[u].orders[S];
				switch (order.command)
				{
					case K_FIND:
						f = findfaction (order.args.shift());

						if (f == null)
						{
							mistake2 (unit,"Faction not found");
							break;
						}
						sparagraph("The address of "+f.id+" is "+f.addr);
						break;
				}
			}
		}
}

function process_leaveEnter()
{
	var r,u,S,region,unit,order,sh,b,u2;

			/* Leaving and entering buildings and ships */

	log ("Processing leaving and entering orders...");

	for (r in regions) {
		region=regions[r];
		for (u in region.units) {
			unit=region.units[u];
			for (S in unit.orders) {
				order=unit.orders[S];
				switch (order.args.shift())
				{
					case K_BOARD:
						sh = findship (order.args.shift(), region);

						if (sh==null)
						{
							mistake2 (order,"Ship not found");
							break;
						}

						if (!sh.mayboard(u))
						{
							mistake2 (order,"Not permitted to board");
							break;
						}
						unit.leave();
						unit.ship=sh;
						unit.owner=false;
						if(sh.owner==null)
							unit.owner=true;
						break;

					case K_ENTER:
						b = findbuilding (order.args.shift(), region);

						if (b==null)
						{
							mistake2 (order,"Building not found");
							break;
						}

						if (!b.mayenter (u))
						{
							mistake2 (order,"Not permitted to enter");
							break;
						}

						unit.leave();
						unit.building = b;
						unit.owner = false;
						if (b.owner == null)
							unit.owner = true;
						break;

					case K_LEAVE:
						if (region.terrain == T_OCEAN)
						{
							mistake2 (order,"Ship is at sea");
							break;
						}
						unit.leave();
						break;

					case K_PROMOTE:
						u2 = unit.getunit(order.args);

						if (u2==null || u2.no==undefined)
						{
							mistake2 (order,"Unit not found");
							break;
						}

						if (!unit.building && !unit.ship)
						{
							mistake2 (order,"No building or ship to transfer ownership of");
							break;
						}

						if (!unit.owner)
						{
							mistake2 (order,"Not owned by you");
							break;
						}

						if (!u2.accepts(u))
						{
							mistake2 (order,"Unit does not accept ownership");
							break;
						}

						if (unit.building)
						{
							if (u2.building.no != unit.building.no)
							{
								mistake2 (order,"Unit not in same building");
								break;
							}
						}
						else
							if (u2.ship.no != u.ship.no)
							{
								mistake2 (order,"Unit not on same ship");
								break;
							}
						
						unit.owner=false;
						u2.owner=true;
						break;
				}
			}
		}
	}
}

/* Combat */
function process_attack()
{
	var r, region;
	var f, fno, faction, fa, f2;
	var u, unit, u2, u3, unit3, u4; 
	var b, S, order, S2, order2;
	var initial=new Array(2);
	var infront=new Array(2);
	var maxtactics=new Array(2);
	var left=new Array(2);
	var shields=new Array(2);
	var runeswords=new Array(2);
	var leader=new Array(2);
	var litems=new Array(MAXITEMS);
	var toattack=new Array(MAXITEMS);
	var troops;
	var i,j,t,n,k;
	var oldargs;
	var tmp;
	var lmoney;
	var buf,buf2;
	var attacker=new Troop();
	var defender=new Troop();
	var winnercasualties;
	var deadpeasants;
	var dh, reportcasualtiesdh;

	log("Processing ATTACK orders...");

	function terminate(i)
	{
		if(!troops[i].attacked) {
			troops[i].attacked=true;
			toattack[defender.side]--;
		}

		troops[i].status=1;
		left[defender.side]--;
		if(infront[defender.side])
			infront[defender.side]--;
		if(troops[i].runesword)
			runeswords[defender.side]--;
	}

	function validtarget(i)
	{
		return((!troops[i].status) && troops[i].side == defender.side && (!troops[i].behind || !infront[defender.side]));
	}

	function selecttarget()
	{
		var i;

		do {
			i=random(troops.length);
		} while(!validtarget(i));

		return(i);
	}

	function battlerecord(str) {}

	function battlepunit(unit) {}

	function addbattle(faction,str) {}

	function docombatspell(i)
	{
		var j,z,n,m,buf;

		function dozap(n)
		{
			n=lovar(n*(1+attacker.power));
			n=Math.min(n,left[defender.side]);

			buf += ", inflicting "+n+" "+(n==1?"casualty":"casualties");

			while(--n >= 0)
				terminate(selecttarget());
		}

		function canberemoralized(i)
		{
			return((!troops[i].status) && troops[i].side==attacker.side && troops[i].demoralized);
		}

		function canbedemoralized(i)
		{
			return(validtarget(i) && !troops[i].demoralized);
		}

		function canbedazzled(i)
		{
			return(validtarget(i) && !troops[i].dazzled);
		}

		z=troops[i].unit.combatspell;
		buf=troops[i].unit.id+" casts "+spelldata[z].name;

		if(shields[defender.side]) {
			if(random(2)) {
				buf += ", and gets through the shield";
				shields[defender.side] -= 1 + attacker.power;
			}
			else {
				buf += ", but the spell is deflected by the shield!";
				battlerecord(buf);
				return;
			}
		}

		switch(z) {
			case SP_BLACK_WIND:
				dozap(1250);
				break;

			case SP_CAUSE_FEAR:
				if(runeswords[defender.side] && random(2))
					break;
				n=lovar(100*(1+attacker.power));
				m=0;
				for(j=0; j<troops.length; j++) {
					if(canbedemoralized(j))
						m++;
				}

				n=Math.min(n,m);
				buf += ", affecting "+n+" "+(n==1?'person':'people');
			
				while(--n >= 0) {
					do {
						j=random(troops.length);
					} while(!canbedemoralized(j));
					troops[j].demoralized=1;
				}
				break;

			case SP_DAZZLING_LIGHT:
				n=lovar(50*(1+attacker.power));
				m=0;
				for(j=0; j<troops.length; j++) {
					if(canbedazzled(j))
						m++;
				}
				n=Math.min(n,m);
				buf += ", dazzling "+n+" "+(n==1?'person':'people');
			
				while(--n >= 0) {
					do {
						j=random(troops.length);
					} while(!canbedazzled(j));
					troops[j].dazzled=1;
				}
				break;

			case SP_FIREBALL:
				dozap(50);
				break;
			case SP_HAND_OF_DEATH:
				dozap(250);
				break;
			case SP_INSPIRE_COURAGE:
				n=lovar(100*(1+attacker.power));
				m=0;
				for(j=0;j<troops.length;j++) {
					if(canberemoralized(j))
						m++;
				}
				n=Math.min(n,m);
				buf += ", affecting "+n+" "+(n==1?'person':'people');
				while(--n >= 0) {
					do {
						j=random(troops.length);
					} while(!canberemoralized(j));
					troops[j].demoralized=0;
				}
				break;
			case SP_LIGHTNING_BOLT:
				dozap(10);
				break;
			case SP_SHIELD:
				shields[attacker.side] += 1+attacker.power;
				break;
			case SP_SUNFIRE:
				dozap(6250);
				break;
			default:
				throw("Unhandled Spell!");
		}
		buf += '!';
		battlerecord(buf);
	}

	function doshot()
	{
		var ai,di;

		function hits()
		{
			var k;

			function contest(a,d)
			{
				var i;
				var table=[10,25,40];

				i=a-d + 1;
				if(i<0)
					return(random(100) < 1);
				if(i>2)
					return(random(100) < 49);
				return(random(100) < table[i]);
			}

			if(defender.weapon == I_CROSSBOW || defender.weapon == I_LONGBOW)
				defender.skill = -2;
			defender.skill += defender.inside;
			attacker.skill -= (attacker.demoralized + attacker.dazzled);
			defender.skill -= (defender.demoralized + defender.dazzled);
			
			switch(attacker.weapon) {
				case 0:
				case I_SWORD:
					k = contest(attacker.skill, defender.skill);
					break;
				case I_CROSSBOW:
					k = contest(attacker.skill, 0);
					break;
				case I_LONGBOW:
					k = contest(attacker.skill, 2);
					break;
			}

			if(defender.invulnerable && random(10000))
				k = 0;
			if(random(3) < defender.armor)
				k = 0;
			return k;
		}

		do {
			ai=random(troops.length);
		} while(troops[ai].attacked);

		attacker=troops[ai];
		toattack[attacker.side]--;
		defender.side=1-attacker.side;

		troops[ai].dazzled=0;

		if(attacker.unit.no != undefined) {
			if(attacker.behind && infront[attacker.side] && !attacker.missile)
				return;
			if(attacker.shieldstone)
				shields[attacker.side] += 1+attacker.power;
			if(attacker.unit.combatspell >= 0) {
				docombatspell(ai);
				return;
			}
			if(attacker.reload) {
				troops[ai].reload--;
				return;
			}
			if(attacker.weapon==I_CROSSBOW)
				troops[ai].reload=2;
		}

		/* Select defender */
		di = selecttarget();
		defender=troops[di];
		if(defender.side != 1-attacker.side) {
			throw("Defdender side "+defender.side+" is not inverse of attacker side "+attacker.side);
		}

		if(hits())
			terminate(di);
	}

	function reportcasualties(unit)
	{
		var buf;

		if(!unit.dead)
			return;
		if(!reportcasualtiesdh) {
			battlerecord("");
			reportcasualtiesdh=1;
		}
		if(unit.number==1)
			buf=unit.id+" is dead.";
		else {
			if(unit.dead == unit.number)
				buf=unit.id+" is wiped out.";
			else
				buf=unit.id+" loses "+unit.dead+".";
		}
		battlerecord(buf);
	}

	for (r in regions)
	{
		region=regions[r];
		/* Create randomly sorted list of factions */
		fa=new Array();

		for (f in factions)
			fa.push(factions[f]);
		scramble (fa);

		/* Handle each faction's attack orders */

		for (fno in fa)
		{
			f = fa[fno];

			for (u in region.units) {
				unit=region.units[u];
				if (unit.faction.no == f.no) {
					for (S in unit.orders) {
						order=unit.orders[S];
						if (order.command == K_ATTACK)
						{
							u2 = unit.getunit (order.args);

							if (u2.no==undefined && u2.getunitpeasants==undefined)
							{
								mistake2 (order,"Unit not found");
								continue;
							}

							if (u2.no!=undefined && u2.faction.no == f.no)
							{
								mistake2 (order,"One of your units");
								continue;
							}

							if (unit.isallied (u2))
							{
								mistake2 (order,"An allied unit");
								continue;
							}

									/* Draw up troops for the battle */

							for (b in region.buildings)
								region.buildings[b].sizeleft = region.buildings[b].size;

							troops = new Array();
							left[0] = left[1] = 0;
							infront[0] = infront[1] = 0;

									/* If peasants are defenders */

							if (u2.no==undefined)
							{
								for (i in region.peasants)
								{
									t=new Troop();
									troops.push(t);
								}

								left[0] = region.peasants;
								infront[0] = region.peasants;
							}

									/* What units are involved? */

							for (f2 in factions)
								factions[f2].attacking = false;

							for (u3 in region.units) {
								for (S2 in region.units[u3].orders) {
									if (region.units[u3].orders[S2].command == K_ATTACK)
									{
										oldargs=new Array();
										for(tmp in region.units[u3].orders[S2].args) {
											oldargs.push(region.units[u3].orders[S2].args[tmp]);
										}
										u4 = region.units[u3].getunit (region.units[u3].orders[S2].args);

										if ((u2 != undefined && u2.getunitpeasants != undefined) ||
											 (u4.no != undefined && u4.faction.no == u2.faction.no &&
											  !region.units[u3].isallied (u4)))
										{
											region.units[u3].faction.attacking = true;
											region.units[u3].orders.splice(S2,1);
											break;
										}
										else {
											region.units[u3].orders.args=oldargs;
										}
									}
								}
							}

							for (u3 in region.units)
							{
								unit3=region.units[u3];
								unit3.side = -1;

								if (!unit3.number)
									continue;

								if (unit3.faction.attacking)
								{
									unit3.side = 1;
									maketroops (troops,unit3,region.terrain,left,infront,runeswords);
								}
								else if (unit3.isallied (u2))
								{
									unit3.side = 0;
									maketroops (troops,unit3,region.terrain,left,infront,runeswords);
								}
							}

							/* If only one side shows up, cancel */

							if (!left[0] || !left[1])
							{
								troops=[];
								continue;
							}

							/* Set up array of troops */

							troops=scramble(troops);

							initial[0] = left[0];
							initial[1] = left[1];
							shields[0] = 0;
							shields[1] = 0;
							runeswords[0] = 0;
							runeswords[1] = 0;

							lmoney = 0;
							for(tmp in litems)
								litems[tmp]=0;

							/* Initial attack message */

							for (f2 in factions)
							{
								factions[f2].seesbattle = factions[f2].ispresent(r);
								if (factions[f2].seesbattle && factions[f2].battles)
									factions[f2].battles.push('');
							}

							if (u2.no != undefined)
								buf2=u2.id;
							else
								buf2="the peasants";
							buf=unit.id+" attacks "+buf2+" in "+region.id;
							battlerecord(buf);

									/* List sides */

							battlerecord ("");

							battlepunit (u);

							for (u3 in region.units)
								if (region.units[u3].side == 1 && region.units[u3].no != unit.no)
									battlepunit (region.units[u3]);

							battlerecord ("");

							if (u2.no != undefined)
								battlepunit (u2);
							else
							{
								buf="Peasants, number: "+region.peasants;
								for (f2 in factions)
									if (factions[f2].seesbattle)
										sparagraph (factions[f2].battles,buf,4,'-');
							}

							for (u3 in region.units)
								if (region.units[u3].side == 0 && region.units[u3].no != u2.no)
									battlepunit (u3);

							battlerecord ("");

									/* Does one side have an advantage in tactics? */

							maxtactics[0] = 0;
							maxtactics[1] = 0;

							for (i = 0; i < troops.length; i++)
								if (troops[i].unit.no!=undefined)
								{
									j = troops[i].unit.effskill (SK_TACTICS);

									if (maxtactics[troops[i].side] < j)
									{
										leader[troops[i].side] = i;
										maxtactics[troops[i].side] = j;
									}
								}

							attacker.side = -1;
							if (maxtactics[0] > maxtactics[1])
								attacker.side = 0;
							if (maxtactics[1] > maxtactics[0])
								attacker.side = 1;

									/* Better leader gets free round of attacks */

							if (attacker.side >= 0)
							{
										/* Note the fact in the battle report */

								if (attacker.side)
									battlerecord(unit.id+" gets a free round of attacks!");
								else
									if (u2.no != undefined)
										battlerecord (u2.id+" gets a free round of attacks!");
									else
										battlerecord ("The peasants get a free round of attacks!");

										/* Number of troops to attack */

								toattack[attacker.side] = 0;

								for (i = 0; i < troops.length; i++)
								{
									troops[i].attacked = true;

									if (troops[i].side == attacker.side)
									{
										troops[i].attacked = false;
										toattack[attacker.side]++;
									}
								}

										/* Do round of attacks */

								do
									doshot ();
								while (toattack[attacker.side] && left[defender.side]);
							}

									/* Handle main body of battle */

							toattack[0] = 0;
							toattack[1] = 0;

							while (left[defender.side])
							{
										/* End of a round */

								if (toattack[0] == 0 && toattack[1] == 0)
									for (i = 0; i < troops.length; i++)
									{
										troops[i].attacked = true;

										if (!troops[i].status)
										{
											troops[i].attacked = false;
											toattack[troops[i].side]++;
										}
									}

								doshot ();
							}

									/* Report on winner */

							if (attacker.side)
								battlerecord (unit.id+" wins the battle!");
							else
								if (u2.no != undefined)
									battlerecord (u2.id+" wins the battle!");
								else
									battlerecord ("The peasants win the battle!");

									/* Has winner suffered any casualties? */

							winnercasualties = 0;

							for (i = 0; i < troops.length; i++)
								if (troops[i].side == attacker.side && troops[i].status)
								{
									winnercasualties = 1;
									break;
								}

									/* Can wounded be healed? */

							n = 0;

							for (i = 0; i < troops.length &&
											n != initial[attacker.side] -
												  left[attacker.side]; i++)
								if (!troops[i].status && troops[i].canheal)
								{
									k = lovar (50 * (1 + troops[i].power));
									k = Math.min (k,initial[attacker.side] -
													  left[attacker.side] - n);

									battlerecord (troops[i].unit.id+" heals "+k+" wounded.");

									n += k;
								}

							while (--n >= 0)
							{
								do
									i = random(troops.length);
								while (!troops[i].status || troops[i].side != attacker.side);

								troops[i].status = 0;
							}

									/* Count the casualties */

							deadpeasants = 0;

							for (u3 in region.units)
								region.units[u3].dead = 0;

							for (i = 0; i < troops.length; i++)
								if (troops[i].unit)
									troops[i].unit.dead += troops[i].status;
								else
									deadpeasants += troops[i].status;

									/* Report the casualties */

							reportcasualtiesdh = 0;

							if (attacker.side)
							{
								reportcasualties (unit);

								for (u3 in region.units)
									if (region.units[u3].side == 1 && region.units[u3].no != unit.no)
										reportcasualties (region.units[u3]);
							}
							else
							{
								if (u2.no != undefined)
									reportcasualties (u2);
								else
									if (deadpeasants)
									{
										battlerecord ("");
										reportcasualtiesdh = 1;
										battlerecord ("The peasants lose "+deadpeasants+".");
									}

								for (u3 in region.units)
									if (region.units[u3].side == 0 && region.units[u3].no != u2.no)
										reportcasualties (region.units[u3]);
							}

									/* Dead peasants */

							k = region.peasants - deadpeasants;

							j = distribute (region.peasants,k,region.money);
							lmoney += region.money - j;
							region.money = j;

							region.peasants = k;

									/* Adjust units */

							for (u3 in region.units)
							{
								unit3=region.units[u3];
								k = unit3.number - unit3.dead;

										/* Redistribute items and skills */

								if (unit3.side == defender.side)
								{
									j = distribute (unit3.number,k,unit3.money);
									lmoney += unit3.money - j;
									unit3.money = j;

									for (i = 0; i < MAXITEMS; i++)
									{
										j = distribute (unit3.number,k,unit3.items[i]);
										litems[i] += unit3.items[i] - j;
										unit3.items[i] = j;
									}
								}

								for (i = 0; i < MAXSKILLS; i++)
									unit3.skills[i] = distribute (unit3.number,k,unit3.skills[i]);

										/* Adjust unit numbers */

								unit3.number = k;

										/* Need this flag cleared for reporting of loot */

								unit3.n = 0;
							}

									/* Distribute loot */

							for (n = lmoney; n; n--)
							{
								do
									j = random(troops.length);
								while (troops[j].status || troops[j].side != attacker.side);

								if (troops[j].unit)
								{
									troops[j].unit.money++;
									troops[j].unit.n++;
								}
								else
									region.money++;
							}

							for (i = 0; i < MAXITEMS; i++)
								for (n = litems[i]; n; n--)
									if (i <= I_STONE || random(2))
									{
										do
											j = random(troops.length);
										while (troops[j].status || troops[j].side != attacker.side);

										if (troops[j].unit)
										{
											if (!troops[j].unit.litems)
											{
												troops[j].unit.litems = [];
												for(tmp=0; tmp < MAXITEMS; tmp++)
													troops[j].unit.litems.push(0);
											}

											troops[j].unit.items[i]++;
											troops[j].unit.litems[i]++;
										}
									}

									/* Report loot */

							for (f2 in factions)
								factions[f2].dh = 0;

							for (u3 in region.units) {
								unit3=region.units[u3];
								
								if (unit3.n || unit3.litems)
								{
									buf=unit3.id+" finds ";
									dh = 0;

									if (unit3.n)
									{
										buf += "$";
										buf += unit3.n;
										dh = 1;
									}

									if (unit3.litems)
									{
										for (i = 0; i != MAXITEMS; i++)
											if (unit3.litems[i])
											{
												if (dh)
													buf += ", ";
												dh = 1;

												buf += unit3.litems[i];
												buf += " ";

												if (unit3.litems[i] == 1)
													buf += items[i].singular;
												else
													buf += items[i].plural;
											}

										unit3.litems = 0;
									}

									if (!unit3.faction.dh)
									{
										addbattle (unit3.faction,"");
										unit3.faction.dh = 1;
									}

									buf += ".";
									addbattle (unit3.faction,buf);
								}
							}

									/* Does winner get combat experience? */

							if (winnercasualties)
							{
								if (maxtactics[attacker.side] &&
									 !troops[leader[attacker.side]].status)
									troops[leader[attacker.side]].unit.skills[SK_TACTICS] += COMBATEXP;

								for (i = 0; i != troops.length; i++)
									if (troops[i].unit.no != undefined &&
										 !troops[i].status &&
										 troops[i].side == attacker.side)
										switch (troops[i].weapon)
										{
											case I_SWORD:
												troops[i].unit.skills[SK_SWORD] += COMBATEXP;
												break;

											case I_CROSSBOW:
												troops[i].unit.skills[SK_CROSSBOW] += COMBATEXP;
												break;

											case I_LONGBOW:
												troops[i].unit.skills[SK_LONGBOW] += COMBATEXP;
												break;
										}
							}
						}
					}
				}
			}
		}
	}
}

/* Economic orders */
function process_economic()
{
	var r,region,u,unit,S,order,b,u2,event,tmp,i,j,k,n,sh;
	var taxorders,recruitorders,taxed,norders,availmoney;

	log ("Processing economic orders...");

	for (r in regions)
	{
		region=regions[r];
		taxorders = new Array();
		recruitorders = new Array();

		/* DEMOLISH, GIVE, PAY, SINK orders */

		for (u in region.units) {
			unit=region.units[u];
			for (S in unit.orders) {
				order=unit.orders[S];
				switch (order.command)
				{
					case K_DEMOLISH:
						if (unit.building==null)
						{
							mistake2 (order,"Not in a building");
							break;
						}

						if (!unit.owner)
						{
							mistake2 (order,"Building not owned by you");
							break;
						}

						b = unit.building;

						for (u2 in region.units)
							if (region.units[u2].building.no == b.no)
							{
								region.units[u2].building = null;
								region.units[u2].owner = false;
							}
						
						event=new Event();
						event.text=unit.id+" demolishes "+b;
						event.location=region;
						event.unit=unit;
						event.target=b;
						region.reportevent(event);
						removenofromarray(region.buildings,b);
						break;

					case K_GIVE:
						u2 = unit.getunit(order.args);

						if (u2==null || (u2.no==undefined && u2.getunit0==undefined))
						{
							mistake2 (order,"Unit not found");
							break;
						}

						if (u2.no != undefined && !u2.accepts (u))
						{
							mistake2 (order,"Unit does not accept your gift");
							break;
						}
						
						tmp=order.args.shift();
						i = findspell (tmp);

						if (i >= 0)
						{
							if (u2==null)
							{
								mistake2 (order,"Unit not found");
								break;
							}

							if (!unit.spells[i])
							{
								mistake2 (order,"Spell not found");
								break;
							}

							if (spelldata[i].level > (u2.effskill(SK_MAGIC) + 1) / 2)
							{
								mistake2 (order,"Recipient is not able to learn that spell");
								break;
							}

							u2.spells[i] = 1;

							event=new Event();
							event.text=unit.id+" gives "+u2.id+" the "+spelldata[i].name+" spell.";
							event.unit=unit;
							event.target=u2;
							event.location=region;
							unit.faction.events.push(event);
							if(unit.faction.no != u2.faction.no)
								u2.faction.events.push(event);
						}
						else
						{
							n = tmp;
							i = finditem (order.args.shift());

							if (i < 0)
							{
								mistake2 (order,"Item not recognized");
								break;
							}

							if (n > unit.items[i])
								n = unit.items[i];

							if(n < 0)
							{
								mistake2 (order,"Negative amounts not allowed");
								break;
							}
							
							if (n == 0)
							{
								mistake2 (order,"Item not available");
								break;
							}

							unit.items[i] -= n;

							event=new Event();
							event.location=region;
							event.unit=unit;

							if (u2.getunit0)
							{
								if (n == 1)
									event.text=unit.id+" discards 1 "+items[i].singular+".";
								else
									event.text=unit.id+" discards "+n+" "+items[i].plural+".";

								unit.faction.events.push(event);
								break;
							}

							event.target=u2;
							u2.items[i] += n;

							if(n==1)
								event.text=unit.id+" gives "+u2.id+" 1 "+items[i].singular+".";
							else
								event.text=unit.id+" gives "+u2.id+" "+n+" "+items[i].plural+".";

							unit.faction.events.push(event);
							if(unit.faction.no != u2.faction.no)
								u2.faction.events.push(event);
						}

						break;

					case K_PAY:
						u2 = unit.getunit (order.args);

						if (u2==null)
						{
							mistake2 (order,"Unit not found");
							break;
						}

						n = order.args.shift();

						if (n > unit.money)
							n = unit.money;

						if(n < 0)
						{
							mistake2 (order,"Negative amounts not allowed");
							break;
						}

						if (n == 0)
						{
							mistake2 (order,"No money available");
							break;
						}

						unit.money -= n;
						event=new Event();
						event.location=region;
						event.unit=unit;
						event.target=u2;

						if (u2.no != undefined)
						{
							u2.money += n;
							event.text=unit.id+" pays "+u2.id+" $"+n+".";
							if(unit.faction.no != u2.faction.no)
								u2.faction.events.push(event);
						}
						else {
							if (u2.getunitpeasants != undefined)
							{
								region.money += n;
								event.text=unit.id+" pays the peasants $"+n+".";
							}
							else
								event.text=unit.id+" discards $"+n+".";
						}
						unit.faction.events.push(event);
						break;

					case K_SINK:
						if (unit.ship==null)
						{
							mistake2 (order,"Not on a ship");
							break;
						}

						if (!unit.owner)
						{
							mistake2 (order,"Ship not owned by you");
							break;
						}

						if (region.terrain == T_OCEAN)
						{
							mistake2 (order,"Ship is at sea");
							break;
						}

						sh = unit.ship;

						for (u2 in region.units)
							if (region.units[u2].ship.no == sh.no)
							{
								region.units[u2].ship = null;
								region.units[u2].owner = false;
							}
						event=new Event();
						event.location=region;
						event.unit=unit;
						event.target=sh;
						event.text=unit.id+" sinks "+sh.id+".";
						region.reportevent(event);

						removenofromarray(region.ships,sh);
						break;
				}
			}
		}

		/* TRANSFER orders */

		for (u in region.units) {
			unit=region.units[u];
			for (S in unit.orders) {
				order=unit.orders[S];
				switch (order.command)
				{
					case K_TRANSFER:
						u2 = unit.getunit (order.args);

						if (u2 != null && u2.no != undefined)
						{
							if (!u2.accepts(unit))
							{
								mistake2 (order,"Unit does not accept your gift");
								break;
							}
						}
						else if (u2==null || u2.getunitpeasants==undefined)
						{
							mistake2 (order,"Unit not found");
							break;
						}

						n = order.args.shift();

						if(n < 0)
						{
							mistake2 (order,"Negative amounts not allowed");
							break;
						}

						if (n > unit.number)
							n = unit.number;

						if (n == 0)
						{
							mistake2 (order,"No people available");
							break;
						}

						if (unit.skills[SK_MAGIC] && u2.getunitpeasants != undefined)
						{
							k = u2.faction.magicians;
							if (u2.faction.no != unit.faction.no)
								k += n;
							if (!u2.skills[SK_MAGIC])
								k += u2.number;

							if (k > 3)
							{
								mistake2 (order,"Only 3 magicians per faction");
								break;
							}
						}

						k = unit.number - n;

						for (i = 0; i != MAXSKILLS; i++)
						{
							j = distribute (unit.number,k,unit.skills[i]);
							if (u2.getunitpeasants==undefined)
								u2.skills[i] += u.skills[i] - j;
							u.skills[i] = j;
						}

						unit.number = k;

						event=new Event();
						event.unit=unit;
						event.location=region;
						event.target=u2;
						if (u2.getunitpeasants==undefined)
						{
							u2.number += n;

							for (i = 0; i != MAXSPELLS; i++)
								if (unit.spells[i] && u2.effskill (SK_MAGIC) / 2 >= spelldata[i].level)
									u2.spells[i] = 1;

							event.text=unit.id+" transfers ";
							if (k)
								event.text += n+' ';
							event.text += "to "+u2.id;
							if (unit.faction.no != u2.faction.no)
								u2.faction.events.push(event);
						}
						else
						{
							region.peasants += n;

							if (k)
								event.text=unit.id+" disbands "+n+".";
							else
								event.text=unit.id+" disbands.";
						}

						unit.faction.push(event);
						break;
				}
			}
		}

		/* TAX orders */

		for (u in region.units)
		{
			unit=region.units[u];
			taxed = false;

			for (S in unit.orders) {
				order=unit.orders[S];
				switch (order.command)
				{
					case K_TAX:
						if (taxed)
							break;
						tmp=false;

						n = unit.armedmen;

						if (!n)
						{
							mistake2 (order,"Unit is not armed and combat trained");
							break;
						}

						for (u2 in region.units)
							if (region.units[u2].guard && region.units[u2].number && !region.units[u2].admits(u))
							{
								mistake2 (order,region.units[u2].name+" is on guard");
								tmp=true;
								break;
							}

						if (tmp)
							break;

						order.qty=n*TAXINCOME;
						taxorders.push(order);
						taxed = true;
						break;
				}
			}
		}

		/* Do taxation */

		for (u in region.units)
			region.units[u].n = -1;

		norders = 0;

		for (S in taxorders)
			norders += parseInt(taxorders[S].qty / 10);

		i = 0;

		for (S in taxorders) {
			for (j = parseInt(taxorders[S].qty / 10); j; j--)
			{
				taxorders[S].n = 0;
				i++;
			}
		}

		scramble(taxorders);

		for (i = 0; i < taxorders.length && region.money > 10; i++, region.money -= 10)
		{
			taxorders[i].unit.money += 10;
			taxorders[i].unit.n += 10;
		}

		for (u in region.units) {
			if (region.units[u].n >= 0)
			{
				event=new Event();
				event.unit=region.units[u];
				event.location=region;
				event.text=region.units[u].id+" collects $"+region.units[u].n+" in taxes.";
				region.units[u].faction.events.push(event);
			}
		}

		/* GUARD 1, RECRUIT orders */

		for (u in region.units)
		{
			unit=region.units[u];

			availmoney = unit.money;

			for (S in unit.orders) {
				order=unit.orders[S];
				switch (order.command)
				{
					case K_GUARD:
						if (order.args.shift())
							unit.guard = true;
						break;

					case K_RECRUIT:
						if (availmoney < RECRUITCOST)
							break;

						n = order.args.shift();

						if (unit.skills[SK_MAGIC] && unit.faction.magicians + n > 3)
						{
							mistake2 (order,"Only 3 magicians per faction");
							break;
						}

						n = Math.min (n,parseInt(availmoney / RECRUITCOST));

						order.qty=n;
						recruitorders.push(order);

						availmoney -= order.qty * RECRUITCOST;
						break;
				}
			}
		}

		/* Do recruiting */

		expandorders(region,recruitorders);

		for (i = 0, n = parseInt(region.peasants / RECRUITFRACTION); i != recruitorders.length && n; i++, n--)
		{
			recruitorders[i].unit.number++;
			region.peasants--;
			recruitorders[i].unit.money -= RECRUITCOST;
			region.money += RECRUITCOST;
			recruitorders[i].unit.n++;
		}

		for(u in region.units) {
			if(region.units[u].n >= 0) {
				event=new Event();
				event.location=region;
				event.unit=region.units[u];
				event.text=region.units[u].id+" recruits "+region.units[u].n+".";
				region.units[u].faction.events.push(event);
			}
		}
	}
}

/* QUIT orders */
function process_quit()
{
	var r,u,S,unit,order;

	log("Processing QUIT orders...");

	for (r in regions)
		for (u in regions[r].units) {
			unit=regions[r].units[u];
			for (S in unit.orders) {
				order=unit.orders[S];
				switch (order.command)
				{
					case K_QUIT:
						if(unit.faction.no != order.args.shift())
						{
							mistake2 (order,"Correct faction number not given");
							break;
						}

						unit.faction.destroy();
						break;
				}
			}
		}
}

/* Set production orders */
function set_production()
{
	var r,region,u,unit,S,order,i;

	log("Setting production orders...");

	for (r in regions) {
		region=regions[r];
		for (u in region.units)
		{
			unit=region.units[u];
			unit.thisorder=unit.lastorder;
			unit.thisorder.args=new Array();
			for(i in unit.lastorder.args)
				unit.thisorder.args.push(unit.lastorder.args[i]);

			for (S in unit.orders) {
				order=unit.orders[S];
				switch (order.command)
				{
					case K_BUILD:
					case K_CAST:
					case K_ENTERTAIN:
					case K_MOVE:
					case K_PRODUCE:
					case K_RESEARCH:
					case K_SAIL:
					case K_STUDY:
					case K_TEACH:
					case K_WORK:
						unit.thisorder=order;
						break;
				}
			}

			switch (unit.thisorder)
			{
				case K_MOVE:
				case K_SAIL:
					break;

				default:
					unit.lastorder=unit.thisorder;
					unit.lastorder.args=new Array();
					for(i in unit.thisorder.args)
						unit.lastorder.args.push(unit.thisorder.args[i]);
			}
		}
	}
}

/* MOVE orders */
function process_move()
{
	var r,region,u,unit,S,order,r2,event;

	log("Processing MOVE orders...");

	for (r in regions) {
		region=regions[r];
		for (u=0; u<region.units.length; u++)	// TODO: This array changes as we work through it!
		{
			unit=region.units[u];
			switch (unit.thisorder)
			{
				case K_MOVE:
					r2 = region.movewhere(unit.thisorder.args.shift());

					if (r2==null)
					{
						mistakeu (unit,"Direction not recognized");
						break;
					}

					if (region.terrain == T_OCEAN)
					{
						mistakeu (unit,"Currently at sea");
						break;
					}

					if (r2.terrain == T_OCEAN)
					{
						event=new Event();
						event.location=region;
						event.unit=unit;
						event.target=r2;
						event.text=unit.id+" discovers that ("+r2.x+","+r2.y+") is ocean.";
						unit.faction.events.push(event);
						break;
					}

					if (!unit.canmove)
					{
						mistakeu (unit,"Carrying too much weight to move");
						break;
					}

					unit.leave();
					removenofromarray(region.units,unit);
					addnotoarray(r2,unit);
					unit.region=r2;
					unit.thisorder=null;

					event=new Event();
					event.location=region;
					event.unit=unit;
					event.target=r2;
					event.text=unit.id+" ";
					if(unit.canride)
						event.text += "rides";
					else
						event.text += "walks";
					event.text += " from "+region.id+" to "+r2.id+".";
					unit.faction.events.push(event);
					u--;
					break;
			}
		}
	}
}

/* SAIL orders */
function process_sail()
{
	var r,r2,region,u,unit,event,u2,unit2,next;

	log("Processing SAIL orders...");

	for (r in regions) {
		region=regions[r];
		for (u=0; u<region.units.length; u++)	// TODO: This array changes as we work through it!
		{
			unit=region.units[u];
			if(u < region.units.length-1)
				next=region.units[u+1].no;
			else
				next=null;

			switch (unit.thisorder.command)
			{
				case K_SAIL:
					r2 = region.movewhere(unit.thisorder.args.shift());

					if (r2==null)
					{
						mistakeu (unit,"Direction not recognized");
						break;
					}

					if (unit.ship==null)
					{
						mistakeu (unit,"Not on a ship");
						break;
					}

					if (unit.owner)
					{
						mistakeu (unit,"Ship not owned by you");
						break;
					}

					if (r2.terrain != T_OCEAN && !r2.iscoast)
					{
						event=new Event();
						event.unit=unit;
						event.location=region;
						event.target=r2;
						event.text=unit.id+" discovers that ("+r2.x+","+r2.y+") is inland.";
						unit.faction.events.push(event);
						break;
					}

					if (unit.ship.left)
					{
						mistakeu (unit,"Ship still under construction");
						break;
					}

					if (!unit.ship.cansail)
					{
						mistakeu (unit,"Too heavily loaded to sail");
						break;
					}

					removenofromarray(region.ships, unit.ship);
					addnotoarray(r2.ships, unit.ship);
					unit.ship.region=r2;

					for (u2=0; u2<region.units.length; u2++)	// TODO: This array changes!
					{
						unit2=region.units[u2];
						if(unit2.ship.no==unit.ship.no) {
							if(next != null) {
								if(unit2.no==next)
									u++;
									if(u>region.units.length)
										next=null;
									else
										next=region.units.length;
							}
							removenofromarray(region.units, unit2);
							addnotoarray(r2.units, unit2);
							unit2.thisorder=null;
							unit2.region=r2;
							if(next != null) {
								while(region.units[u].no != next)
									u--;
							}
							u2--;
						}
					}
					break;
			}
		}
	}
}

function buildship(region,unit,sh)
{
	var n,event;

	n = unit.number * unit.effskill (SK_SHIPBUILDING);
	n = Math.min (n,sh.left);
	n = Math.min (n,unit.items[I_WOOD]);
	sh.left -= n;
	unit.items[I_WOOD] -= n;

	unit.skills[SK_SHIPBUILDING] += n * 10;

	event=new Event();
	event.location=region;
	event.unit=unit;
	event.target=sh;
	event.text=unit.id+" adds "+n+" to "+sh.id+".";
	unit.faction.events.push(event);
}

function createship(region,unit,i)
{
	var sh;

	if (!unit.effskill (SK_SHIPBUILDING))
	{
		mistakeu (unit,"You don't have the skill");
		return;
	}

	sh = new Ship();

	sh.type = i;
	sh.left = shiptypes[i].cost;

	do
	{
		sh.no++;
		sh.name="Ship "+sh.no;
	}
	while (findship (sh.no)!=null);

	region.ships.push(sh);

	unit.leave ();
	unit.ship = sh;
	unit.owner = true;
	buildship(region,unit,sh);
}

/* Do production orders */
function process_production()
{
	var r,region,u,unit,i,b,sh,entertainorders,workorders,produceorders;
	var n,event,o,j,teaching,teachunits,tmp,u2,k;

	log("Processing production orders...");

	for (r in regions)
	{
		region=regions[r];
		if (region.terrain == T_OCEAN)
			continue;

		entertainorders = new Array();
		workorders = new Array();
		produceorders = new Array(4);
		for(j=0; j<4; j++)
			produceorders[j]=new Array();

		for (u in region.units) {
			unit=region.units[u];
			switch (unit.thisorder.command)
			{
				case K_BUILD:
					switch (unit.thisorder.args.shift())
					{
						case K_BUILDING:
							if (!unit.effskill (SK_BUILDING))
							{
								mistakeu (unit,"You don't have the skill");
								break;
							}

							if (!unit.items[I_STONE])
							{
								mistakeu (unit,"No stone available");
								break;
							}

							b = findbuilding (unit.thisorder.args.shift(),r);

							if (b==null)
							{
								b = new Building();

								do
								{
									b.no++;
									b.name="Building "+b.no;
								}
								while (findbuilding(b.no)!=null);
								
								region.buildings.push(b);

								unit.leave();
								unit.building = b;
								unit.owner = true;
							}

							n = unit.number * unit.effskill (SK_BUILDING);
							n = Math.min (n,unit.items[I_STONE]);
							b.size += n;
							unit.items[I_STONE] -= n;

							unit.skills[SK_BUILDING] += n * 10;
							
							event=new Event();
							event.location=region;
							event.unit=unit;
							event.target=b;
							event.text=unit.id+" adds "+n+" to "+b.id+".";
							unit.faction.events.push(event);
							break;

						case K_SHIP:
							if (!unit.effskill (SK_SHIPBUILDING))
							{
								mistakeu (unit,"You don't have the skill");
								break;
							}

							if (!unit.items[I_WOOD])
							{
								mistakeu (unit,"No wood available");
								break;
							}

							sh = findship (unit.thisorder.args.shift(),r);

							if (sh == null)
							{
								mistakeu (unit,"Ship not found");
								break;
							}

							if (!sh.left)
							{
								mistakeu (unit,"Ship is already complete");
								break;
							}

							buildship(region,unit,sh);
							break;

						case K_LONGBOAT:
							createship(region,unit,SH_LONGBOAT);
							break;

						case K_CLIPPER:
							createship(region,unit,SH_CLIPPER);
							break;

						case K_GALLEON:
							createship(region,unit,SH_GALLEON);
							break;

						default:
							mistakeu (unit,"Order not recognized");
					}

					break;

				case K_ENTERTAIN:
					o = new Order();
					o.command=K_ENTERTAIN;
					o.unit = unit;
					o.qty = unit.number * unit.effskill (SK_ENTERTAINMENT) * ENTERTAININCOME;
					entertainorders.push(o);
					break;

				case K_PRODUCE:
					i = unit.thisorder.args.shift();

					if (isNaN(i) || items[i].skill==undefined)
					{
						mistakeu (unit,"Item not recognized");
						break;
					}

					n = unit.effskill(items[i].skill);

					if (n == 0)
					{
						mistakeu (unit,"You don't have the skill");
						break;
					}

					if (i == I_PLATE_ARMOR)
						n /= 3;

					n *= unit.number;

					if (items[items[i].rawmaterial]==undefined)
					{
						o=new Order();
						o.command=K_PRODUCE;
						o.unit=unit;
						o.qty=n * terrains[region.terrain].productivity[i];
						o.args.push(i);
						produceorders[i].push(o);
					}
					else
					{
						n = Math.min (n,unit.items[items[i].rawmaterial]);

						if (n == 0)
						{
							mistakeu (unit,"No material available");
							break;
						}

						unit.items[i] += n;
						unit.items[items[i].rawmaterial] -= n;

						event=new Event();
						event.location=region;
						event.unit=unit;
						if (n == 1)
							event.text=unit.id+" produces 1 "+items[i].singular+".";
						else
							event.text=unit.id+" produces "+n+" "+items[i].plural+".";
						unit.faction.events.push(event);
					}

					unit.skills[items[i].skill] += unit.number * PRODUCEEXP;
					break;

				case K_RESEARCH:
					if (unit.effskill (SK_MAGIC) < 2)
					{
						mistakeu (unit,"Magic skill of at least 2 required");
						break;
					}

					i = unit.thisorder.args.shift();

					if (i > unit.effskill (SK_MAGIC) / 2)
					{
						mistakeu (unit,"Insufficient Magic skill - highest available level researched");
						i = 0;
					}

					if (i == 0)
						i = unit.effskill (SK_MAGIC) / 2;

					k = 0;

					for (j = 0; j < MAXSPELLS; j++)
						if (spelldata[j].level == i && !unit.spells[j])
							k = 1;

					if (k == 0)
					{
						if (unit.money < 200)
						{
							mistakeu (unit,"Insufficient funds");
							break;
						}

						for (n = unit.number; n; n--)
							if (unit.money >= 200)
							{
								unit.money -= 200;
								unit.skills[SK_MAGIC] += 10;
							}

						event=new Event();
						event.location=region;
						event.unit=unit;
						event.text=unit.id+" discovers that no more level "+i+" spells exist.";
						unit.faction.events.push(event);
						break;
					}

					for (n = unit.number; n; n--)
					{
						if (unit.money < 200)
						{
							mistakeu (unit,"Insufficient funds");
							break;
						}

						do
							j = random(MAXSPELLS);
						while (spelldata[j].level != i || unit.spells[j] == 1);

						if (!unit.faction.seendata[j])
						{
							unit.faction.seendata[j] = 1;
							unit.faction.showdata[j] = 1;
						}

						if (unit.spells[j] == 0)
						{
							event=new Event();
							event.location=region;
							event.unit=unit;
							event.text=unit.id+" discovers the "+spelldata[j].name+" spell.";
							unit.faction.events.push(event);
						}

						unit.spells[j] = 2;
						unit.skills[SK_MAGIC] += 10;
					}

					for (j = 0; j < MAXSPELLS; j++)
						if (unit.spells[j] == 2)
							unit.spells[j] = 1;
					break;

				case K_TEACH:
					teaching = unit.number * 30 * TEACHNUMBER;
					teachunits=new Array();

					while(1) {
						tmp=unit.getunit(unit.thisorder.args);
						if(tmp==null)
							break;
						if(tmp.no != undefined)
							teachunits.push(tmp);
					}

					for (j = 0; j < teachunits.length; j++)
					{
						u2 = teachunits[j];

						if (u2==null)
						{
							mistakeu (unit,"Unit not found");
							continue;
						}

						if (!u2.accepts(unit))
						{
							mistakeu (unit,"Unit does not accept teaching");
							continue;
						}

						i = u2.thisorder.args[0];

						if (u2.thisorder.command != K_STUDY || isNaN(i) || i < 0 || i >= MAXSKILLS)
						{
							mistakeu (unit,"Unit not studying");
							continue;
						}

						if (unit.effskill(i) <= u2.effskill(i))
						{
							mistakeu (unit,"Unit not studying a skill you can teach it");
							continue;
						}

						n = (u2.number * 30) - u2.learning;
						n = Math.min (n,teaching);

						if (n == 0)
							continue;

						u2.learning += n;
						teaching -= unit.number * 30;

						event=new Event();
						event.location=region;
						event.unit=unit;
						event.text=unit.id+" teaches "+u2.id+" "+skillnames[i]+".";
						unit.faction.events.push(event);
						if (u2.faction != unit.faction)
							u2.faction.events.push(event);
					}

					break;

				case K_WORK:
					o = new Order();
					o.command=K_WORK;
					o.unit = unit;
					o.qty = unit.number * terrains[region.terrain].foodproductivity;
					workorders.push(o);
					break;
			}
		}

				/* Entertainment */

		expandorders(region,entertainorders);

		for (i = 0, n = region.money / ENTERTAINFRACTION; i < entertainorders.length && n; i++, n--)
		{
			entertainorders[i].unit.money++;
			region.money--;
			entertainorders[i].unit.n++;
		}

		for (u in region.units)
			if (region.units[u].n >= 0)
			{
				event=new Event();
				event.location=region;
				event.unit=region.units[u];
				event.text=region.units[u].id+" earns $"+region.units[u].n+" entertaining.";
				region.units[u].faction.events.push(event);

				region.units[u].skills[SK_ENTERTAINMENT] += 10 * region.units[u].number;
			}

				/* Food production */

		expandorders (region,workorders);

		for (i = 0, n = terrains[region.terrain].maxfoodoutput; i < workorders.length && n; i++, n--)
		{
			workorders[i].unit.money++;
			workorders[i].unit.n++;
		}

		region.money += Math.min (n,region.peasants * terrains[region.terrain].foodproductivity);

		for (u in region.units) {
			unit=region.units[u];
			if (unit.n >= 0)
			{
				event=new Event();
				event.location=region;
				event.unit=region.units[u];
				event.text=region.units[u].id+" earns $"+region.units[u].n+" performing manual labor.";
				region.units[u].faction.events.push(event);
			}
		}

				/* Production of other primary commodities */

		for (i = 0; i != 4; i++)
		{
			expandorders (region,produceorders[i]);

			for (j = 0, n = terrains[region.terrain].maxoutput[i]; j != produceorders[i].length && n; j++, n--)
			{
				produceorders[i][j].unit.items[i]++;
				produceorders[i][j].unit.n++;
			}

			for (u in region.units) {
				unit=region.units[u];
				if (unit.n >= 0)
				{
					event=new Event();
					event.location=region;
					event.unit=region.units[u];
					if(unit.n==1)
						event.text=region.units[u].id+" produces 1 "+items[i].singular+".";
					else
						event.text=region.units[u].id+" produces "+region.units[u].n+" "+items[i].plural+".";
					region.units[u].faction.events.push(event);
				}
			}
		}
	}
}

/* Study skills */
function process_study()
{
	var r,region,u,unit,event,i;

	log("Processing STUDY orders...");

	for (r in regions) {
		region=regions[r];
		if (region.terrain != T_OCEAN) {
			for (u in region.units) {
				unit=region.units[u];
				switch (unit.thisorder.command)
				{
					case K_STUDY:
						i = unit.thisorder.args.ahift();

						if (isNaN(i) || i < 0 || i>=MAXSKILLS)
						{
							mistakeu (unit,"Skill not recognized");
							break;
						}

						if (i == SK_TACTICS || i == SK_MAGIC)
						{
							if (unit.money < STUDYCOST * unit.number)
							{
								mistakeu (unit,"Insufficient funds");
								break;
							}

							if (i == SK_MAGIC && !unit.skills[SK_MAGIC] &&
								 unit.faction.magicians + unit.number > 3)
							{
								mistakeu (unit,"Only 3 magicians per faction");
								break;
							}

							unit.money -= STUDYCOST * unit.number;
						}

						event=new Event();
						event.location=region;
						event.unit=region.units[u];
						event.text=unit.id+" studies "+skillnames[i]+".";
						unit.faction.events.push(event);

						unit.skills[i] += (unit.number * 30) + unit.learning;
						break;
				}
			}
		}
	}
}

/* Ritual spells, and loss of spells where required */
function process_cast()
{
	var r,region,u,unit,event,i,j,n,f,u2,r2,region2,u3;

	log("Processing CAST orders...");

	for (r in regions)
	{
		region=regions[r];
		for (u in region.units)
		{
			unit=region.units[u];

			for (i = 0; i < MAXSPELLS; i++)
				if (unit.spells[i] && spelldata[i].level > (unit.effskill (SK_MAGIC) + 1) / 2)
					unit.spells[i] = 0;

			if (unit.combatspell >= 0 && !unit.cancast (unit.combatspell))
				unit.combatspell = -1;
		}

		if (region.terrain != T_OCEAN) {
			for (u in region.units) {
				unit=region.units[u];
				switch (unit.thisorder.command)
				{
					case K_CAST:
						i = unit.thisorder.args.shift();

						if (isNaN(i) || i < 0 || i>=MAXSPELLS || !unit.cancast (i))
						{
							mistakeu (unit,"Spell not found");
							break;
						}

						j = spelldata[i].spellitem;

						if (j != undefined && j >= 0)
						{
							if (unit.money < 200 * spelldata[i].level)
							{
								mistakeu (unit,"Insufficient funds");
								break;
							}

							n = Math.min (unit.number,parseInt(unit.money / (200 * spelldata[i].level)));
							unit.items[j] += n;
							unit.money -= n * 200 * spelldata[i].level;
							unit.skills[SK_MAGIC] += n * 10;

							event=new Event();
							event.location=region;
							event.unit=unit;
							event.text=unit.id+" casts "+spelldata[i].name+".";
							unit.faction.events.push(event);
							break;
						}

						if (unit.money < 50)
						{
							mistakeu (unit,"Insufficient funds");
							break;
						}

						switch (i)
						{
							case SP_CONTAMINATE_WATER:
								n = unit.cancast (SP_CONTAMINATE_WATER);
								n = Math.min (n,parseInt(unit.money / 50));

								unit.money -= n * 50;
								unit.skills[SK_MAGIC] += n * 10;

								n = lovar (n * 50);
								n = Math.min (n,region.peasants);

								if (!n)
									break;

								region.peasants -= n;

								for (f in factions)
								{
									j = f.cansee (unit);

									if (j)
									{
										event=new Event();
										event.location=region;
										event.unit=unit;
										if (j == 2)
											event.text=unit.id+" contaminates the water supply in "+region.id+", causing "+n+" peasants to die.";
										else
											event.text=n+" peasants die in "+region.id+" from drinking contaminated water.";
										factions[f].events.push(event);
									}
								}

								break;

							case SP_TELEPORT:
								u2 = unit.getunit(unit.thisorder.args);

								if (u2==null || u2.no==undefined)
								{
									mistakeu (unit,"Unit not found");
									break;
								}

								if (!u2.admits (unit))
								{
									mistakeu (u,"Target unit does not provide vector");
									break;
								}

								regionloop:
								for (r2 in regions)
								{
									region2=regions[r2];
									for (u3 in regions[r2].units)
										if (regions[r2].units[u3].no == u2.no)
											break regionloop;
								}

								n = unit.cancast (SP_TELEPORT);
								n = Math.min (n,parseInt(unit.money / 50));

								unit.money -= n * 50;
								unit.skills[SK_MAGIC] += n * 10;

								n *= 10000;

								for (;;)
								{
									u3 = unit.getunit(unit.thisorder.args);

									if (u3 != null && u3.getunit0 != undefined)
										break;

									if (u3 == null || u3.no == undefined)
									{
										mistakeu (unit,"Unit not found");
										continue;
									}

									if (!u3.accepts (unit))
									{
										mistakeu (unit,"Unit does not accept teleportation");
										continue;
									}

									i = u3.itemweight + u3.horseweight + (u3.number * 10);

									if (i > n)
									{
										mistakeu (unit,"Unit too heavy");
										continue;
									}

									u3.leave ();
									n -= i;
									removenofromarray(region.units, u3);
									addnotoarray(region2.units,u3);
									u3.building = u2.building;
									u3.ship = u2.ship;
								}

								event=new Event();
								event.location=region;
								event.unit=unit;
								event.target=region2;
								event.text=unit.id+" casts Teleport.";
								unit.faction.events.push(event);
								break;

							default:
								mistakeu (unit,"Spell not usable with CAST command");
						}

						break;
				}
			}
		}
	}
}

/* Population growth, dispersal and food consumption */
function process_demographics()
{
	var r,region,n,u,unit,i,event;

	log("Processing demographics...");

	for (r in regions)
	{
		region=regions[r];
		if (region.terrain != T_OCEAN)
		{
			for (n = region.peasants; n; n--)
				if (random(100) < POPGROWTH)
					region.peasants++;

			n = parseInt(region.money / MAINTENANCE);
			region.peasants = Math.min (region.peasants,n);
			region.money -= region.peasants * MAINTENANCE;

			for (n = region.peasants; n; n--)
				if (random(100) < PEASANTMOVE)
				{
					i = random(4);

					if (region.connect[i].terrain != T_OCEAN)
					{
						region.peasants--;
						region.connect[i].immigrants++;
					}
				}
		}

		for (u in region.units)
		{
			unit=region.units[u];

			getmoney (r,unit,unit.number * MAINTENANCE);
			n = unit.money / MAINTENANCE;

			if (unit.number > n)
			{
				event=new Event();
				event.location=region;
				event.unit=unit;
			
				if (n)
					event.text=unit.id+" loses "+n+" to starvation.";
				else
					event.text=unit.id+" starves to death.";
				unit.faction.events.push(event);

				for (i = 0; i < MAXSKILLS; i++)
					unit.skills[i] = distribute (unit.number,n,unit.skills[i]);

				unit.number = n;
			}

			unit.money -= unit.number * MAINTENANCE;
		}
	}
}

function processorders()
{
	var f,r;

	process_form();
	process_instant();
	process_find();
	process_leaveEnter();
	process_attack();		// TODO Combat!
	process_economic();
	process_quit();

	/* Remove players who haven't sent in orders */

	for (f in factions)
		if (turn - factions[f].lastorders > ORDERGAP)
			factions[f].destroy();

	/* Clear away debris of destroyed factions */

	removeempty();
	removenullfactions();

	set_production();
	process_move();
	process_sail();
	process_production();
	process_study();
	process_cast();
	process_demographics();

	removeempty ();

	for (r in regions)
		regions[r].peasants += regions[r].immigrants;

			/* Warn players who haven't sent in orders */

	for (f in factions)
		if (turn - factions[f].lastorders == ORDERGAP)
			factions[f].messages.push("Please send orders next turn if you wish to continue playing.");
}

function writesummary()
{
	var sfile=new File(game_dir+"summary");
	sfile.open("w");
	sfile.write(gamesummary());
	sfile.close();
}

function readorders()
{
	var orderfiles=directory(game_dir+'orders/*.'+turn);
	var orderfile;
	var orders;
	var f;
	var ordereval,order;
	var unitno,unit;
	var fano;
	var tmp;

	for(orderfile in orderfiles) {
		tmp=file_getname(orderfile).split(/\./,2);
		fano=tmp[0];
		f=new File(orderfile);
		f.open("r");
		ordereval=js.eval(f.readAll.join('\n'));
		f.close();
		for(unitno in ordereval) {
			unit=findunit(unitno);
			if(unit == null || unit.faction.no != fano) {
				mistakestr(fano,"Unit "+unitno+" is not on of your units!");
				continue;
			}
			unit.faction.lastorders=turn;
			for(order in ordereval[unitno]) {
				unit.orders.push(ordereval[unitno][order]);
			}
		}
	}
}

function reports()
{
	var f,f2,rf,tmp,tmp2,prop,rep,i,prop2,arr,r,region,u,unit,ret;

	for(f in factions) {
		ret={};
		ret.faction={};
		tmp=ret.faction;
		for(prop in factions[f]) {
			switch(prop) {
				/* Keep Unchanged */
				case 'no':
				case 'name':
				case 'lastorders':
				case 'money':
				case 'id':
				case 'addr':
					tmp[prop]=factions[f][prop];
					break;

				/* Clean up sub-objects */
				case 'mistakes':	// TODO: Clean up sub-objects
				case 'messages':	// TODO: Clean up sub-objects
				case 'battles':		// TODO: Clean up sub-objects
				case 'events':
					tmp[prop]=[];
					for(i in factions[f][prop]) {
						if(typeof(factions[f][prop][i])=='object') {
							tmp[prop][i]={};
							for(prop2 in factions[f][prop][i]) {
								if(factions[f][prop][i][prop2] !== null && typeof(factions[f][prop][i][prop2])=='object') {
									if(factions[f][prop][i][prop2].id != undefined) {
										tmp[prop][i][prop2]=factions[f][prop][i][prop2].id;
									}
									else if(factions[f][prop][i][prop2].no != undefined) {
										tmp[prop][i][prop2]=factions[f][prop][i][prop2].no;
									}
									else {
										log("Unknown object type: ["+prop+"]["+i+"]["+prop2+"]");
										tmp[prop][i][prop2]='Unknown Type';
									}
								}
								else
									tmp[prop][i][prop2]=factions[f][prop][i][prop2];
							}
						}
						else
							tmp[prop][i]=factions[f][prop][i];
					}
					break;

				/* Convert Other Faction */
				case 'accept':
				case 'allies':
				case 'admit':
					/* Convert to just an array of IDs */
					for(i in factions[f][prop])
						tmp[prop][i]=factions[f][prop][i].id;
					break;
			}
		}
		ret.factions=[];
		r=ret.factions;
		for(f2 in factions) {
			if(factions[f2].no==factions[f].no)
				continue;
			tmp={};
			for(prop in factions[f2]) {
				switch(prop) {
					/* Keep Unchanged */
					case 'no':
					case 'name':
					case 'id':
					case 'addr':
						tmp[prop]=factions[f2][prop];
						break;
				}
			}
			arr.push(tmp);
		}

		ret.regions=[];
		arr=ret.regions;
		for(r in regions) {
			region=regions[r];
			if(factions[f].ispresent(region)) {
				tmp={};
				for(prop in region) {
					switch(prop) {
						/* Keep Unchanged */
						case 'x':
						case 'y':
						case 'name':
						case 'id':
						case 'terrain':
						case 'peasants':
						case 'money':
							tmp[prop]=region[prop];
							break;

						/* Modify - remove region and modify owner */
						case 'buildings':
						case 'ships':
							tmp[prop]=[];
							for(i in region[prop]) {
								tmp[prop][i]={};
								for(prop2 in region[prop][i]) {
									switch(prop2) {
										/* Keep Unchanged */
										case 'no':
										case 'name':
										case 'display':
										case 'size':
										case 'id':
										case 'type':
											tmp[prop][i][prop2]=region[prop][i][prop2];
											break;
										/* Replace with ID */
										case 'owner':
											tmp[prop][i][prop2]=region[prop][i][prop2].id;
											break;
									}
								}
							}
							break;
					}
				}
				tmp.units=[];
				for(u in region.units) {
					unit=region.units[u];
					switch(factions[f].cansee(unit)) {
						case 1:	/* Limited visibility */
							tmp2={};
							for(prop in unit) {
								switch(prop) {
									/* Keep Unchanged */
									case 'name':
									case 'no':
									case 'id':
									case 'number':
									case 'guard':
									case 'items':
									case 'display':
										tmp2[prop]=unit[prop];
										break;
								}
							}
							tmp.units.push(tmp2);
							break;
						case 2: /* Full visibility */
							tmp2={};
							/* Only for self */
							if(factions[f].no==unit.faction.no) {
								tmp2.behind=unit.behind;
								tmp2.money=unit.money;
								tmp2.skills=unit.skills;
								tmp2.spells=unit.spells;
								tmp2.combatspell=unit.combatspell;
							}
							for(prop in unit) {
								switch(prop) {
									/* Keep Unchanged */
									case 'name':
									case 'no':
									case 'id':
									case 'number':
									case 'guard':
									case 'items':
									case 'display':
										tmp2[prop]=unit[prop];
										break;
									/* Modify - use ID */
									case 'faction':
										tmp2[prop]=unit[prop].id;
								}
							}
							tmp.units.push(tmp2);
							break;
					}
				}
				arr.push(tmp);
			}
			else {
				/* Not present... */
				tmp={};
				for(prop in region) {
					switch(prop) {
						/* Keep Unchanged */
						case 'x':
						case 'y':
						case 'name':
						case 'terrain':
							tmp[prop]=region[prop];
							break;
					}
				}
				arr.push(tmp);
			}
		}

		rf=new File(game_dir+"reports/"+f+"."+turn);
		rf.open("w");
		rf.write(ret.toSource());
	}
}

function processturn()
{
	turn++;
	readorders();
	processorders();
	reports();
	//writesummary();
	//writegame();
}

if(argc < 2) {
	alert("Invalid usage!");
	exit(1);
}
switch(argv[1]) {
	case 'process':
		initgame(argv[0]);
		processturn();
		addplayers();
		break;
	case 'init':
		initgame(argv[0]);
		addplayers();
		break;
}
writesummary();
writegame();

