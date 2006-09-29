/* SMMCFG.JS */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */
/* Converted to JS by Deuce */

load("sbbsdefs.js");
load("uifcdefs.js");
load("../xtrn/smm/smmvars.js");
load("../xtrn/smm/smmdefs.js");

function bail(code)
{
	uifc.bail();
	exit(code);
}

var str;
//var i,j,k,file,dflt,sysop_level;
var i;
var j;
var k;
var file;
var dflt;
var sysop_level;
var opt=new Array();
var read_cfg=false;

uifc.init("Synchronet Match Maker v"+SMM_VER);

file = new File("smm.ini");
if (!file.open("r+", false)) {
	/* No smm.ini file, check for and use old smm.cfg */
	file = new File("../xtrn/smm/smm.cfg");
	if(file.open("r", true)) {
		read_cfg=true;
	}
	else {
		file = new File("smm.ini");
		if (!(file.open("w+",false))) {
			uifc.msg("Error opening smm.ini");
			bail(1);
		}
	}
}

if(read_cfg) {
	/* Conversion of old SMM.CFG format to new variables */
	purity_ars = "";
	profile_ars = "";
	wall_ars = "";
	telegram_ars = "";
	que_ars = "";
	sysop_ars = "";

	str=file.readln();
	if(parseInt(str))
		purity_ars += format("AGE %u ",parseInt(str));
	str=file.readln();
	if(parseInt(str))
		profile_ars += format("AGE %u ",parseInt(str));
	str=file.readln();
	if(parseInt(str))
		profile_ars += format("LEVEL %u ",parseInt(str));
	str=file.readln();
	str.toUpperCase;
	var flags=str.split(/\s*/);
	str=flags.join("$F1");
	if(str.length) {
		str="$F1"+str;
		profile_ars += str + " ";
	}
	str=file.readln();
	str.toUpperCase;
	flags=str.split(/\s*/);
	str=flags.join("$F2");
	if(str.length) {
		str="$F2"+str;
		profile_ars += str + " ";
	}
	str=file.readln();
	str.toUpperCase;
	flags=str.split(/\s*/);
	str=flags.join("$F3");
	if(str.length) {
		str="$F3"+str;
		profile_ars += str + " ";
	}
	str=file.readln();
	str.toUpperCase;
	flags=str.split(/\s*/);
	str=flags.join("$F4");
	if(str.length) {
		str="$F4"+str;
		profile_ars += str + " ";
	}
	str=file.readln();
	profile_cdt=parseInt(str);
	str=file.readln();
	telegram_cdt=parseInt(str);
	str=file.readln();
	auto_update=parseInt(str);
	str=file.readln();
	notify_user=parseInt(str);
	str=file.readln();
	profile_cdt=parseInt(str);
	str=file.readln();	/* regnum */
	str=file.readln();
	if(parseInt(str))
		telegram_ars += format("LEVEL %u ",parseInt(str));
	str=file.readln();
	if(parseInt(str))
		que_ars += format("LEVEL %u ",parseInt(str));
	str=file.readln();
	if(parseInt(str))
		wall_ars += format("LEVEL %u ",parseInt(str));
	str=file.readln();
	wall_cdt=parseInt(str);
	str=file.readln();
	que_cdt=parseInt(str);
	str=file.readln();	/* zmodem send */
	str=file.readln();
	smm_misc=parseInt(str);
	system_name=file.readln();
	str=file.readln();	/* local view */
	str=file.readln();
	if(parseInt(str))
		sysop_ars += format("LEVEL %u ",parseInt(str));
	str=file.readln();
	if(parseInt(str))
		wall_ars += format("AGE %u ",parseInt(str));
	str=file.readln();
	age_split=parseInt(str);
}
else {
	purity_ars=file.iniGetValue(null, "PurityARS", "AGE 18");
	profile_ars=file.iniGetValue(null, "ProfileARS", "");
	wall_ars=file.iniGetValue(null,"WallARS","");
	telegram_ars=file.iniGetValue(null,"TelegramARS","");
	que_ars=file.iniGetValue(null,"QuestionnaireARS","");
	sysop_ars=file.iniGetValue(null,"SysopARS","SYSOP");
	auto_update=file.iniGetValue(null,"AutoUpdate",30);
	notify_user=file.iniGetValue(null,"NotifyUser",1);
	profile_cdt=file.iniGetValue(null,"ProfileCredits",0);
	telegram_cdt=file.iniGetValue(null,"TelegramCredits",0);
	wall_cdt=file.iniGetValue(null,"WallCredits",0);
	que_cdt=file.iniGetValue(null,"QuestionnaireCredits",0);
	if(file.iniGetValue(null,"Metric",false))
		smm_misc |= SMM_METRIC;
	system_name=format("%.25s",file.iniGetValue(null,"SystemName",system.name.toUpperCase()));
	age_split=file.iniGetValue(null,"AgeSplit",0);
}
file.close();

dflt = 0;
while (1) {
	uifc.help_text = " Synchronet Match Maker Configuration \r\n\r\n"
		+"Move through the various options using the arrow keys.  Select the\r\n"
		+"highlighted options by pressing ENTER.\r\n\r\n";
	j = 0;
	opt[j++]=format("%-40.40s %s", "System Name", system_name);
	opt[j++]=format("%-40.40s %s","Wall ARS",wall_ars);
	opt[j++]=format("%-40.40s %s","Add Profile ARS",profile_ars);
	opt[j++]=format("%-40.40s %s","Purity Test ARS",purity_ars);
	opt[j++]=format("%-40.40s %ldk"
		,format("Credit %s for Adding Profile"
			,(profile_cdt > 0) ? "Bonus" : "Cost")
		,profile_cdt > 0 ? profile_cdt / 1024 : (-profile_cdt) / 1024);
	opt[j++]=format("%-40.40s %ldk"
		,format("Credit %s for Sending Telegram"
			,telegram_cdt > 0 ? "Bonus" : "Cost")
		,telegram_cdt > 0 ? telegram_cdt / 1024 : (-telegram_cdt) / 1024);
	opt[j++]=format("%-40.40s %ldk"
		,format("Credit %s for Writing on the Wall"
			,wall_cdt > 0 ? "Bonus" : "Cost")
	    ,wall_cdt > 0 ? wall_cdt / 1024 : (-wall_cdt) / 1024);
	opt[j++]=format("%-40.40s %ldk"
		,format("Credit %s for Reading Questionnaire"
			,que_cdt > 0 ? "Bonus" : "Cost")
		,que_cdt > 0 ? que_cdt / 1024 : (-que_cdt) / 1024);
	opt[j++]=format("%-40.40s %s", "ARS to Send Telegram",telegram_ars);
	opt[j++]=format("%-40.40s %s", "ARS to Read Questionnaires",que_ars);
	opt[j++]=format("%-40.40s %s", "Minor Segregation (Protection) Age"
		,age_split ? format("%u",age_split) : "Disabled");
	opt[j++]=format("%-40.40s %s", "Sysop ARS", sysop_ars);
	opt[j++]=format("%-40.40s %s", "Auto-Update Profiles"
	    ,auto_update ? format("%u",auto_update) : "Disabled");
	opt[j++]=format("%-40.40s %s", "Notify User of Activity"
	    ,notify_user ? format("%u",notify_user) : "Disabled");
	opt[j++]=format("%-40.40s %s", "Use Metric System"
		,smm_misc & SMM_METRIC ? "Yes" : "No");
	opt.length=j;
	j=uifc.list(WIN_ORG | WIN_MID | WIN_ACT | WIN_ESC, 0, 0, 60, dflt, dflt
			,"Synchronet Match Maker Configuration", opt);
	switch (j) {
		case 0:
			uifc.help_text = " System Name \r\n\r\n"
				+"This is your BBS name. Once you have configured your BBS name here, you\r\n"
				+"will not be able to change it, without losing all of your local users'\r\n"
				+"profiles in your database.\r\n"
				+"\r\n"
				+"It is highly recommended that you do not change your BBS name here,\r\n"
				+"even if you decide to change your actual BBS name in the future.\r\n"
				+"\r\n"
				+"All BBS names in a match maker network must be unique.";
			system_name=uifc.input(WIN_MID, 0, 0, "System Name"
				   ,system_name, 40, K_EDIT | K_UPPER);
			break;
		case 1:
			uifc.help_text = " Wall ARS \r\n\r\n"
					+"This ARS allows you to specify which users can access the wall"
			wall_ars=uifc.input(WIN_L2R | WIN_SAV, 0, 0,
							   "Wall ARS",wall_ars, 256, K_EDIT | K_UPPER);
			break;
		case 2:
			uifc.help_text = " Add Profile ARS \r\n\r\n"
					+"This ARS allows you to specify which users can create profiles"
			profile_ars=uifc.input(WIN_L2R | WIN_SAV, 0, 0,
							   "Add Profile ARS",profile_ars, 256, K_EDIT | K_UPPER);
			break;
		case 3:
			uifc.help_text = " Purity Test ARS \r\n\r\n"
					+"This ARS allows you to specify which users can take the purity test"
			purity_ars=uifc.input(WIN_L2R | WIN_SAV, 0, 0,
							   "Add Profile ARS",purity_ars, 256, K_EDIT | K_UPPER);
			break;
		case 4:
			uifc.help_text = " Credit Adjustment for Adding Profile \r\n\r\n"
				+ "You can have Synchronet Match Maker either give credits to or take\r\n"
				+ "credits from the user for adding a profile to the database.";
			opt[0]="Add Credits";
			opt[1]="Remove Credits";
			opt.length=2;
			i = uifc.list(WIN_L2R | WIN_BOT | WIN_ACT, 0, 0, 0, 1, 1
				  ,"Credit Adjustment for Adding Profile", opt);
			if (i == -1)
				break;
			str=uifc.input(WIN_MID, 0, 0, "Credits (K=1024)"
				   ,format("%ld", profile_cdt < 0 ? -profile_cdt : profile_cdt), 10, K_EDIT | K_UPPER);
			if (str.indexOf('K')!=-1)
				profile_cdt = parseInt(str) * 1024;
			else
				profile_cdt = parseInt(str);
			if (i == 1)
				profile_cdt = -profile_cdt;
			break;
		case 5:
			uifc.help_text = " Credit Adjustment for Sending Telegram \r\n\r\n"
				+ "You can have Synchronet Match Maker either give credits to or take\r\n"
				+ "credits from the user for sending a telegram to another user in SMM.";
			opt[0]="Add Credits";
			opt[1]="Remove Credits";
			opt.length=2;
			i = uifc.list(WIN_L2R | WIN_BOT | WIN_ACT, 0, 0, 0, 1, 1
				,"Credit Adjustment for Sending Telegram", opt);
			if (i == -1)
				break;
			str=uifc.input(WIN_MID, 0, 0, "Credits (K=1024)"
				   ,format("%ld", telegram_cdt < 0 ? -telegram_cdt : telegram_cdt), 10, K_EDIT | K_UPPER);
			if (str.indexOf('K')!=-1)
				telegram_cdt = parseInt(str) * 1024;
			else
				telegram_cdt = parseInt(str);
			if (i == 1)
				telegram_cdt = -telegram_cdt;
			break;
		case 6:
			uifc.help_text = " Credit Adjustment for Writing on the Wall \r\n\r\n"
				+ "You can have Synchronet Match Maker either give credits to or take\r\n"
				+ "credits from the user for writing on the wall.";
			opt[0]="Add Credits";
			opt[1]="Remove Credits";
			opt.length=2;
			i = uifc.list(WIN_L2R | WIN_BOT | WIN_ACT, 0, 0, 0, 1, 1
			 ,"Credit Adjustment for Writing on the Wall", opt);
			if (i == -1)
				break;
			str=uifc.input(WIN_MID, 0, 0, "Credits (K=1024)"
				   ,format(str, "%ld", wall_cdt < 0 ? -wall_cdt : wall_cdt), 10, K_EDIT | K_UPPER);
			if (str.indexOf('K')!=-1)
				wall_cdt = parseInt(str) * 1024;
			else
				wall_cdt = parseInt(str);
			if (i == 1)
				wall_cdt = -wall_cdt;
			break;
		case 7:
			uifc.help_text = " Credit Adjustment for Reading Questionnaire \r\n\r\n"
				+ "You can have Synchronet Match Maker either give credits to or take\r\n"
				+ "credits from the user when reading another user's questionnaire";
			opt[0]="Add Credits";
			opt[1]="Remove Credits";
			opt.length=2;
			i = uifc.list(WIN_L2R | WIN_BOT | WIN_ACT, 0, 0, 0, 1, 1
			,"Credit Adjustment for Reading Questionnaire", opt);
			if (i == -1)
				break;
			str=uifc.input(WIN_MID, 0, 0, "Credits (K=1024)"
				   ,format("%ld", que_cdt < 0 ? -que_cdt : que_cdt), 10, K_EDIT | K_UPPER);
			if (str.indexOf('K'))
				que_cdt = parseInt(str) * 1024;
			else
				que_cdt = parseInt(str);
			if (i == 1)
				que_cdt = -que_cdt;
			break;
		case 8:
			uifc.help_text = " ARS to Send Telegrams \r\n\r\n"
				+ "Use this option to restrict the sending of telegrams to users of a\r\n"
				+ "who match a specific ARS string.";
			telegram_ars=uifc.input(WIN_MID, 0, 0, "ARS to Send Telegrams"
				   ,telegram_ars, 256, K_EDIT | K_UPPER);
			break;
		case 9:
			uifc.help_text = " ARS to Read Questionnaires \r\n\r\n"
				"Users will only be allowed to read other users' questionnaires if\r\n"
				"they match this ARS string.";
			que_ars=uifc.input(WIN_MID, 0, 0, "ARS to Read Questionnaires"
				   ,que_ars, 256, K_EDIT | K_UPPER);
			break;
		case 10:
			helpbuf =
				" Minor Segregation (Protection) Age \r\n\r\n"
				"This option (if enabled) separates all users into two groups:\r\n"
				"\r\n"
				"Minors: Those users below the specified age (normally 18)\r\n"
				"Adults: Those users at or above the specified age\r\n"
				"\r\n"
				"If enabled, adults cannot see minors' profiles or send telegrams to\r\n"
				"minors and vice versa.\r\n"
				"\r\n"
				"If disabled, all users can see eachother's profiles regardless of age.\r\n";
			str=uifc.input(WIN_MID, 0, 0, "Minor Segregation Age (0=Disabled)"
				   ,format("%u",age_split), 2, K_EDIT | K_NUMBER);
			age_split = parseInt(str);
			break;
		case 11:
			uifc.help_text = " Sysop ARS \r\n\r\n"
				+ "Users matching this ARS will be given access to sysop\r\n"
				+ "commands in the match maker.";
			sysop_ars=uifc.input(WIN_MID, 0, 0, "Sysop ARS", sysop_ars, 256, K_EDIT | K_UPPER);
			break;

		case 12:
			uifc.help_text = " Auto-Update Profiles \r\n\r\n"
				+"If you would like to have Synchronet Match Maker automatically send\r\n"
				+"a network update message for a user that is active in SMM, but hasn't\r\n"
				+"actually made any changes to his or her profile, set this option to the\r\n"
				+"number of days between Auto-Updates (e.g. 30 days is a good value).\r\n"
				+"\r\n"
				+"Setting this option to 0 disables this feature.";
			str=uifc.input(WIN_MID, 0, 0, "Auto-Update Profiles (in Days)"
				   ,format("%u", auto_update), 3, K_EDIT | K_NUMBER);
			auto_update = parseInt(str);
			break;
		case 13:
			uifc.help_text = " Notify User of Activity \r\n\r\n"
				+"If you would like to have Synchronet Match Maker automatically send\r\n"
				+"a message to a specific user (most commonly the sysop) whenever a user\r\n"
				+"adds a profile to the database or sends a telegram from SMM, set this\r\n"
				+"option to the number of that user (e.g. 1 would indicate user #1).\r\n"
				+"\r\n"
				+"Setting this option to 0 disables this feature.";
			str=uifc.input(WIN_MID, 0, 0, "Notify User Number (0=disabled)"
				   ,format("%u", notify_user), 5, K_EDIT | K_NUMBER);
			notify_user = parseInt(str);
			break;
		case 14:
			uifc.help_text = " Use Metric System \r\n\r\n"
				+"If you wish to use centimeters and kilograms instead of inches and\r\n"
				+"pounds for height and weight measurements, set this option to Yes.";
			opt[0]="Yes";
			opt[1]="No";
			opt.length=2;
			i = uifc.list(WIN_MID | WIN_ACT, 0, 0, 0, smm_misc & SMM_METRIC ? 0 : 1, 0
				  ,"Use Metric Measurement System", opt);
			if (i == -1)
				break;
			if (i == 1)
				smm_misc &= ~SMM_METRIC;
			else
				smm_misc |= SMM_METRIC;
			break;

		case -1:
			uifc.help_text = " Save Configuration File \r\n\r\n"
				+"Select Yes to save the config file, No to quit without saving,\r\n"
				+"or hit  ESC  to go back to the menu.\r\n\r\n";
			opt[0]="Yes";
			opt[1]="No";
			opt.length=2;
			i = uifc.list(WIN_MID, 0, 0, 0, 0, 0, "Save Config File", opt);
			if (i == -1)
				break;
			if (i)
				bail(0);
			file = new File("smm.ini");
			if (!(file.open("w+",false))) {
				uifc.msg("Error opening smm.ini\r\n");
				bail(1);
			}
			file.iniSetValue(null, "PurityARS", purity_ars);
			file.iniSetValue(null, "ProfileARS", profile_ars);
			file.iniSetValue(null,"WallARS",wall_ars);
			file.iniSetValue(null,"TelegramARS",telegram_ars);
			file.iniSetValue(null,"QuestionnaireARS",que_ars);
			file.iniSetValue(null,"SysopARS",sysop_ars);
			file.iniSetValue(null,"AutoUpdate",auto_update);
			file.iniSetValue(null,"NotifyUser",notify_user);
			file.iniSetValue(null,"ProfileCredits",profile_cdt);
			file.iniSetValue(null,"TelegramCredits",telegram_cdt);
			file.iniSetValue(null,"WallCredits",wall_cdt);
			file.iniSetValue(null,"QuestionnaireCredits",que_cdt);
			file.iniSetValue(null,"Metric",smm_misc & SMM_METRIC ? true : false);
			file.iniSetValue(null,"SystemName",system_name);
			file.iniSetValue(null,"AgeSplit",age_split);
			file.close();
			bail(0);
	}
	if(j != -1)
		dflt=j;
}
