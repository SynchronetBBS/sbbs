var PlayerProperties=[
			{
				 prop:"UserNumber"
				,name:"User Number"
				,type:"Integer"
				,def:(user == undefined?0:user.number)
			}
			,{
				 prop:"Points"
				,name:"Pointes"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Alias"
				,name:"Alias"
				,type:"String:42"
				,def:(user == undefined?"":user.alias)
			}
			,{
				 prop:"LastOnDay"
				,name:"Date Last On"
				,type:"Date"
				,def:system.datestr()
			}
			,{
				 prop:"KilledBy"
				,name:"Killed By"
				,type:"SignedInteger"
				,def:0
			}
			,{
				 prop:"TurnsLeft"
				,name:"Turns Remaining"
				,type:"Integer"
				,def:Settings.TurnsPerDay
			}
			,{
				 prop:"Sector"
				,name:"Sector"
				,type:"Integer"
				,def:1
			}
			,{
				 prop:"Fighters"
				,name:"Fighters"
				,type:"Integer"
				,def:Settings.StartingFighters
			}
			,{
				 prop:"Holds"
				,name:"Holds"
				,type:"Integer"
				,def:Settings.StartingHolds
			}
			,{
				 prop:"Commodities"
				,name:"Commodities"
				,type:"Array:3:Integer"
				,def:[0,0,0]
			}
			,{
				 prop:"Credits"
				,name:"Credits"
				,type:"Integer"
				,def:Settings.StartingCredits
			}
			,{
				 prop:"TeamNumber"
				,name:"Team Number"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"LastIn"
				,name:"Sector Last In"
				,type:"Integer"
				,def:1
			}
			,{
				 prop:"Online"
				,name:"Online"
				,type:"Boolean"
				,def:false
			}
			,{
				 prop:"Ported"
				,name:"Ported"
				,type:"Boolean"
				,def:false
			}
			,{
				 prop:"Landed"
				,name:"Landed"
				,type:"Boolean"
				,def:false
			}
		];

var SectorProperties = [
			{
				 prop:"Warps"
				,name:"Warps"
				,type:"Array:6:Integer"
				,def:[0,0,0,0,0,0]
			}
			,{
				 prop:"Port"
				,name:"Port"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Planet"
				,name:"Planet"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Fighters"
				,name:"Fighters"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"FighterOwner"
				,name:"Fighters Owner"
				,type:"SignedInteger"
				,def:0
			}
		];

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

var PlanetProperties = [
			{
				 prop:"Name"
				,name:"Name"
				,type:"String:42"
				,def:""
			}
			,{
				 prop:"Created"
				,name:"Planet has been created"
				,type:"Boolean"
				,def:false
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
				 prop:"LastUpdate"
				,name:"Planet last produced"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Sector"
				,name:"Location of planet"
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

var PlayerMessageProperties = [
			{
				 prop:"Read"
				,name:"Message has been read"
				,type:"Boolean"
				,def:true
			}
			,{
				 prop:"To"
				,name:"Player the message is to"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"From"
				,name:"Player the message is from"
				,type:"SignedInteger"
				,def:0
			}
			,{
				 prop:"Destroyed"
				,name:"Number of Fighters Destroyed"
				,type:"Integer"
				,def:0
			}
		];

var RadioMessageProperties = [
			{
				 prop:"Read"
				,name:"Message has been read"
				,type:"Boolean"
				,def:false
			}
			,{
				 prop:"To"
				,name:"Player the message is to"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"From"
				,name:"Player the message is from"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Message"
				,name:"Test of the message"
				,type:"String:74"
				,def:""
			}
		];

var TeamProperties = [
			{
				 prop:"Password"
				,name:"Team Password"
				,type:"String:4"
				,def:""
			}
			,{
				 prop:"Members"
				,name:"Team Members"
				,type:"Array:4:Integer"
				,def:[0,0,0,0]
			}
			,{
				 prop:"Size"
				,name:"Number of members"
				,type:"Integer"
				,def:0
			}
		];

var CabalProperties = [
			{
				 prop:"Sector"
				,name:"Sector"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Size"
				,name:"Size"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Goal"
				,name:"Goal"
				,type:"Integer"
				,def:0
			}
		];
