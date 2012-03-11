var DefaultTeam = {
	Password:'',
	Members:[]
};

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

function TeamMenu()
{
	function TeamHelp() {
		console.crlf();
		if(player.TeamNumber > 0)
			console.printfile(fname("team-member.asc"));
		else
			console.printfile(fname("team-none.asc"));
	}

	TeamHelp();

	while(1) {
		var values=new Array();
		if(player.TeamNumber > 0)
			values=['1','2','3','4','5','6','7','?'];
		else
			values=['1','2','3','?'];
		console.crlf();
		console.attributes="W";
		console.write("Team Command (?=help)? ");
		switch(InputFunc(values)) {
			case '?':
				TeamHelp();
				break;
			case '1':
				return;
			case '2':
				CreateTeam();
				break;
			case '3':
				JoinTeam();
				break;
			case '4':
				QuitTeam();
				break;
			case '5':
				TeamTransfer("Credits");
				break;
			case '6':
				TeamTransfer("Fighters");
				break;
			case '7':
				LocateTeam();
				break;
		}
	}
}

function LocateTeam()
{
	console.writeln("Locating Other Team members.");
	console.crlf();
	console.writeln("Name                                     Sector");
	console.writeln("====================                     ======");
	var count=0;
	var i;
	for(i=1;i<players.length; i++) {
		var otherplayer=players.Get(i);
		if(otherplayer.Record!=player.Record
				&& otherplayer.UserNumber!=0
				&& otherplayer.TeamNumber==player.TeamNumber) {
			count++;
			console.write(format("%-41s ",otherplayer.Alias));
			if(otherplayer.KilledBy != 0)
				console.write(" Dead!");
			else
				console.write(otherplayer.Sector);
			console.crlf();
		}
	}
	console.crlf();
	if(count==0)
		console.writeln("None Found");
}

function CreateTeam()
{
	if(player.TeamNumber > 0) {
		console.writeln("You already belong to a Team! ");
		console.writeln("You must quit a team to form another Team.");
		return(false);
	}
	var password='';
	while(password.length != 4) {
		console.writeln("To form a Team you will need to input a 4 character Team Password");
		console.write("Enter Password [ENTER]=quit? ");
		password=console.getstr(4);
		if(password=='')
			return(false);
		if(password.length != 4)
			console.writeln("Password MUST be 4 characters");
	}
	console.crlf();
	console.writeln("REMEMBER YOUR TEAM PASSWORD!");
	var team=null;
	var teamNum=-1;
	var teamLen=db.read('tw2','teams.length',LOCK_READ);
	for(i=1; i<teamLen; i++) {
		team=db.read('tw2','teams.'+i,LOCK_READ);
		if(team.Members.length > 0) {
			teamNum=i;
			break;
		}
		team=null;
	}
	if(team==null) {
		team=eval(DefaultTeam.toSource());
	}
	team.Password=password;
	team.Members[0]=player.Record;
	db.push('tw2','teams',team,LOCK_WRITE);
	/* Now find the record we just wrote... */
	if(teamNum==-1) {
		teamLen=db.read('tw2','teams.length',LOCK_READ);
		for(i=1; i<teamLen; i++) {
			team=db.read('tw2','teams.'+i,LOCK_READ);
			if(team.Members.length > 0 && team.Members[0]==player.Record) {
				teamNum=i;
				break;
			}
		}
	}
	player.TeamNumber=teamNum;
	player.Put();
	console.attributes="HYI";
	db.push('tw2','log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:player.Alias+" Created Team "+teamNum+" the " /* TNAM$ */},LOCK_WRITE);
	console.writeln("Team number [" + teamNum + "] CREATED!");
	console.attributes="HK";
	return(true);
}

function JoinTeam()
{
	var teamLen=db.read('tw2','teams.length',LOCK_READ);

	if(player.TeamNumber > 0) {
		console.writeln("You already belong to a Team! ");
		console.writeln("You must quit one team to join another Team.");
		return(false);
	}

	console.write("What Team number do you wish to join (0=quit)? ");
	var tnum=InputFunc([{min:0,max:(teamLen-1)}]);
	if(!(tnum > 0 && tnum < teamLen))
		return(false);

	var team=db.read('tw2','teams.'+tnum,LOCK_READ);
	if(team.Members.length > 3) {
		console.writeln("The Team you picked has a maximum amount of members!");
		return(false);
	}
	console.write("Please enter Password to Join Team #"+tnum+" ? ");
	var pass=console.getstr(4);
	if(pass != team.Password) {
		console.writeln("Invalid Password entered!");
		return(false);
	}
	var i;
	db.lock('tw2','teams.'+tnum,LOCK_WRITE);
	team=db.read('tw2','teams.'+tnum);
	if(team.Members.length > 3) {
		console.writeln("The Team you picked has a maximum amount of members!");
		db.unlock('tw2','teams.'+tnum);
		return(false);
	}
	team.Members.push(player.Record);
	player.TeamNumber=tnum;
	db.write('tw2','teams.'+tnum, team);
	db.unlock('tw2','teams.'+tnum);
	player.Put();
	db.push('tw2','log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:player.Alias + " Joined Team "+tnum},LOCK_WRITE);
	console.attributes="HC";
	console.writeln("Your Team info has been recorded!  Have fun!");
	console.attributes="HK";
	return(true);
}

function QuitTeam()
{
	if(player.TeamNumber == 0) {
		console.writeln("You don't belong to a Team!");
		return(false);
	}
	console.write("Are you sure you wish to quit your Team [N]? ");
	if(InputFunc(['Y','N'])=='Y') {
		var teamNum=player.TeamNumber;
		db.lock('tw2','teams.'+teamNum,LOCK_WRITE);
		var team=db.read('tw2','teams.'+teamNum);
		var i;
		for(i=0; i<team.Members.length; i++) {
			if(team.Members[i]==player.Record) {
				team.Members.splice(i,1);
				player.TeamNumber=0;
				player.Put();
				db.write('tw2', 'teams.'+teamNum, team);
				db.unlock('tw2','teams.'+teamNum);
				console.crlf();
				console.attributes="HG";
				console.writeln("You have been removed from Team play");
				console.attributes="HK";
				return(true);
			}
		}
	}
	db.unlock('tw2','teams.'+teamNum);
	console.attributes="HR";
	console.writeln("Corrupt team detected on quit!  Please notify the Sysop!");
	db.push('tw2','log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:"!!! Team "+player.TeamNumber+" is corrupted (quit)!"},LOCK_WRITE);
	return(false);
}

function TeamTransfer(type)
{
	console.write("Transfer "+type+" to another Team member [N]? ");
	if(InputFunc(['Y','N'])!='Y')
		return(false);
	if(player[type] < 1) {
		console.writeln("You don't have any "+type+".");
		return(false);
	}
	var i;

	var otherplayer=null;
	for(i=1;i<players.length; i++) {
		otherplayer=players.Get(i);
		if(otherplayer.Sector==player.Sector
				&& otherplayer.Record!=player.Record
				&& otherplayer.KilledBy!=0
				&& otherplayer.UserNumber!=0
				&& otherplayer.TeamNumber==player.TeamNumber) {
			console.write("Transfer "+type+" to " + otherplayer.Alias + " (Y/[N])? ");
			if(InputFunc(['Y','N'])=='Y')
				break;
		}
		otherplayer=null;
	}
	if(otherplayer==null) {
		console.writeln("There is no one else in this sector");
		return(false);
	}
	while(1) {
		console.writeln("You have" + player[type] + " "+type.toLowercase()+".");
		console.writeln(otherplayer.Alias + " has" + otherplayer[type] + " "+type.toLowercase()+".");
		console.write("How many "+type+" do you wish to Transfer? ");
		var transfer=InputFunc([{min:0,max:player[type]}]);
		if(transfer<1 || transfer > player[type])
			return(false);
		if(type=="Credits") {
			if(otherplayer.Credits + transfer > 25000) {
				console.writeln("You Team mate will have more than 25,000 credits");
				console.write("Do you wish to complete the Transfer [N]? ");
				if(InputFunc(['Y','N'])!='Y')
					return(false);
			}
		}
		else if(type=="Fighters") {
			if(otherplayer.Fighters + transfer > 9999)
				console.writeln("Maximum amount of fighters per player is 9999");
		}
	}
	otherplayer[type] += transfer;
	player[type] -= transfer;
	player.Put();
	otherplayer.Put();
	console.writeln(type+" Transfered!");
	return(true);
}

function InitializeTeams()
{
	uifc.pop("Teams");
	db.write('tw2','teams',[],LOCK_WRITE);
	db.push('tw2','teams',{Excuse:"I hate zero-based arrays, so I'm just stuffing this crap in here"},LOCK_WRITE);
}
