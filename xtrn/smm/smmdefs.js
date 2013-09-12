/* SMMDEFS.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

var SMM_VER="2.11";

var SMM_METRIC =			(1<<0);		/* Use metric measurements */

var USER_DELETED =			(1<<0);		/* 	Bits for user.misc field */
var USER_FROMSMB =			(1<<1);		/* Imported from SMB */
var USER_REQZIP = 			(1<<2);		/* Require mate in zip range */
var USER_REQAGE = 			(1<<3);		/* Require mate in age range */
var USER_REQWEIGHT =		(1<<4);		/* Require mate in weight range */
var USER_REQHEIGHT =		(1<<5);		/* Require mate in height range */
var USER_REQINCOME =		(1<<6);		/* Require mate in income range */
var USER_REQMARITAL = 		(1<<7);		/* Require marital status */
var USER_REQZODIAC =		(1<<8);		/* Require zodiac */
var USER_REQRACE =			(1<<9);		/* Require race */
var USER_REQHAIR =			(1<<10);	/* Require hair */
var USER_REQEYES =			(1<<11);	/* Require eyes */
var USER_FRIEND = 			(1<<12);	/* Seeking same-sex friendship */
var USER_PHOTO =			(1<<13);	/* Photo attached to profile */


var ZODIAC_ARIES =			(1<<0);		/* Mar 21 - Apr 19 */
var ZODIAC_TAURUS =			(1<<1);		/* Apr 20 - May 20 */
var ZODIAC_GEMINI =			(1<<2);		/* May 21 - Jun 20 */
var ZODIAC_CANCER =			(1<<3);		/* Jun 21 - Jul 22 */
var ZODIAC_LEO =			(1<<4);		/* Jul 23 - Aug 22 */
var ZODIAC_VIRGO =			(1<<5);		/* Aug 23 - Sep 22 */
var ZODIAC_LIBRA =			(1<<6);		/* Sep 23 - Oct 22 */
var ZODIAC_SCORPIO =		(1<<7);		/* Oct 23 - Nov 21 */
var ZODIAC_SAGITTARIUS =	(1<<8);		/* Nov 22 - Dec 21 */
var ZODIAC_CAPRICORN =		(1<<9);		/* Dec 22 - Jan 19 */
var ZODIAC_AQUARIUS = 		(1<<10);	/* Jan 20 - Feb 18 */
var ZODIAC_PISCES =			(1<<11);	/* Feb 19 - Mar 20 */

var HAIR_BLONDE = 			(1<<0);
var HAIR_BROWN =			(1<<1);
var HAIR_RED =				(1<<2);
var HAIR_BLACK =			(1<<3);
var HAIR_GREY =				(1<<4);
var HAIR_OTHER =			(1<<5);

var EYES_BLUE =				(1<<0);
var EYES_GREEN =			(1<<1);
var EYES_HAZEL =			(1<<2);
var EYES_BROWN =			(1<<3);
var EYES_OTHER =			(1<<4);

var RACE_WHITE =			(1<<0);
var RACE_BLACK =			(1<<1);
var RACE_HISPANIC =			(1<<2);
var RACE_ASIAN =			(1<<3);
var RACE_AMERINDIAN = 		(1<<4);
var RACE_MIDEASTERN = 		(1<<5);
var RACE_OTHER =			(1<<6);

var MARITAL_SINGLE =		(1<<0);
var MARITAL_MARRIED = 		(1<<1);
var MARITAL_DIVORCED =		(1<<2);
var MARITAL_WIDOWED = 		(1<<3);
var MARITAL_OTHER =			(1<<4);

function question_t()
{
	this.txt="";
	this.answers=0;
	this.allowed=0;
	this.ans=new Array("","","","","","","","","","","","","","","","");
	this.validate=question_t_validate;
	this.pack=question_t_pack;
	this.unpack=question_t_unpack;
}

function questionnaire_t()
{
	var i;

	this.name="";
	this.desc="";
	this.req_age=0;
	this.total_ques=0;
	this.que=new Array();
	for(i=0;i<20;i++)
		this.que[i]=new question_t;
}

function queans_t()
{
	var i;

	this.name="";
	this.self=new Array();
	for(i=0; i<20; i++)
		this.self[i]=0;
	this.pref=new Array();
	for(i=0; i<20; i++)
		this.pref[i]=0;
}

function ixb_t_write(file)
{
	file.writeBin(this.number,4);
	file.writeBin(this.system,4);
	file.writeBin(this.name,4);
	file.writeBin(this.updated,4);
}

function ixb_t_read(file)
{
	this.number=file.readBin(4);
	this.system=file.readBin(4);
	this.name=file.readBin(4);
	this.updated=file.readBin(4);
}

function ixb_t()
{
	this.number=0;
	this.system=0;
	this.name=0;
	this.updated=0;
	this.sizeof=16;
	this.write=ixb_t_write;
	this.read=ixb_t_read;
}

function user_t_write(file)
{
	var str='';
	var i;
	
	str=this.name.toString().substr(0,25);
	while(str.length<26)
		str+=ascii(0);
	file.write(str);
	str=this.realname.toString().substr(0,25);
	while(str.length<26)
		str+=ascii(0);
	file.write(str);
	str=this.system.toString().substr(0,40);
	while(str.length<41)
		str+=ascii(0);
	file.write(str);
	file.writeBin(this.number,4);
	str=this.birth.toString().substr(0,8);
	while(str.length<9)
		str+=ascii(0);
	file.write(str);
	str=this.zipcode.toString().substr(0,10);
	while(str.length<11)
		str+=ascii(0);
	file.write(str);
	str=this.location.toString().substr(0,30);
	while(str.length<31)
		str+=ascii(0);
	file.write(str);
	file.writeBin(this.pref_zodiac,2);
	file.writeBin(this.sex,1);
	file.writeBin(this.pref_sex,1);
	file.writeBin(this.marital,1);
	file.writeBin(this.race,1);
	file.writeBin(this.pref_race,1);
	file.writeBin(this.hair,1);
	file.writeBin(this.pref_hair,1);
	file.writeBin(this.eyes,1);
	file.writeBin(this.pref_eyes,1);
	file.writeBin(this.weight,2);
	file.writeBin(this.height,1);
	file.writeBin(this.min_weight,2);
	file.writeBin(this.max_weight,2);
	file.writeBin(this.min_height,1);
	file.writeBin(this.max_height,1);
	file.writeBin(this.min_age,1);
	file.writeBin(this.max_age,1);
	str=this.min_zipcode.toString().substr(0,10);
	while(str.length<11)
		str+=ascii(0);
	file.write(str);
	str=this.max_zipcode.toString().substr(0,10);
	while(str.length<11)
		str+=ascii(0);
	file.write(str);
	file.writeBin(this.income,4);
	file.writeBin(this.min_income,4);
	file.writeBin(this.max_income,4);
	file.writeBin(this.created,4);
	file.writeBin(this.updated,4);
	str=this.note[0].toString().substr(0,50);
	while(str.length<51)
		str+=ascii(0);
	file.write(str);
	str=this.note[1].toString().substr(0,50);
	while(str.length<51)
		str+=ascii(0);
	file.write(str);
	str=this.note[2].toString().substr(0,50);
	while(str.length<51)
		str+=ascii(0);
	file.write(str);
	str=this.note[3].toString().substr(0,50);
	while(str.length<51)
		str+=ascii(0);
	file.write(str);
	str=this.note[4].toString().substr(0,50);
	while(str.length<51)
		str+=ascii(0);
	file.write(str);
	file.writeBin(this.misc,4);
	file.writeBin(this.lastin,4);
	file.writeBin(this.min_match,1);
	file.writeBin(this.purity,1);
	str=this.mbtype.toString().substr(0,4);
	while(str.length<5)
		str+=ascii(0);
	file.write(str);
	file.writeBin(this.photo,4);
	file.write("PADPADPADPADPADPADPADPADPADPADPADPADPADPADPADPADPADPADPADPAD");
	for(i=0;i<5;i++)
		this.queans[i].write(file);
}

function user_t_read(file)
{
	var str='';
	var i;

	this.name=file.read(26);
	this.name=this.name.substr(0,this.name.indexOf(ascii(0))+1);
	this.realname=file.read(26);
	this.realname=this.realname.substr(0,this.realname.indexOf(ascii(0))+1);
	this.system=file.read(26);
	this.system=this.system.substr(0,this.system.indexOf(ascii(0))+1);
	this.number=file.readBin(4);
	this.birth=file.read(26);
	this.birth=this.birth.substr(0,this.birth.indexOf(ascii(0))+1);
	this.zipcode=file.read(26);
	this.zipcode=this.zipcode.substr(0,this.zipcode.indexOf(ascii(0))+1);
	this.location=file.read(26);
	this.location=this.location.substr(0,this.location.indexOf(ascii(0))+1);
	this.pref_zodiac=file.readBin(2);
	this.sex=file.readBin(1);
	this.pref_sex=file.readBin(1);
	this.marital=file.readBin(1);
	this.race=file.readBin(1);
	this.pref_race=file.readBin(1);
	this.hair=file.readBin(1);
	this.pref_hair=file.readBin(1);
	this.eyes=file.readBin(1);
	this.pref_eyes=file.readBin(1);
	this.weight=file.readBin(2);
	this.height=file.readBin(1);
	this.min_weight=file.readBin(2);
	this.max_weight=file.readBin(2);
	this.min_height=file.readBin(1);
	this.max_height=file.readBin(1);
	this.min_age=file.readBin(1);
	this.max_age=file.readBin(1);
	this.min_zipcode=file.read(26);
	this.min_zipcode=this.min_zipcode.substr(0,this.min_zipcode.indexOf(ascii(0))+1);
	this.max_zipcode=file.read(26);
	this.max_zipcode=this.max_zipcode.substr(0,this.max_zipcode.indexOf(ascii(0))+1);
	this.income=file.readBin(4);
	this.min_income=file.readBin(4);
	this.max_income=file.readBin(4);
	this.created=file.readBin(4);
	this.updated=file.readBin(4);
	this.note[0]=file.read(26);
	this.note[0]=this.note[0].substr(0,this.note[0].indexOf(ascii(0))+1);
	this.note[1]=file.read(26);
	this.note[1]=this.note[0].substr(0,this.note[0].indexOf(ascii(0))+1);
	this.note[2]=file.read(26);
	this.note[2]=this.note[0].substr(0,this.note[0].indexOf(ascii(0))+1);
	this.note[3]=file.read(26);
	this.note[3]=this.note[0].substr(0,this.note[0].indexOf(ascii(0))+1);
	this.note[4]=file.read(26);
	this.note[4]=this.note[0].substr(0,this.note[0].indexOf(ascii(0))+1);
	this.misc=file.readBin(4);
	this.lastin=file.readBin(4);
	this.min_match=file.readBin(1);
	this.purity=file.readBin(1);
	this.mbtype=file.read(26);
	this.mbtype=this.note[0].substr(0,this.note[0].indexOf(ascii(0))+1);
	this.photo=this.readBin(4);
	file.read(60);
	for(i=0;i<5;i++)
		this.queans[i].read(file);
}

function user_t()
{
	this.name="";
	this.realname="";
	this.system="";
	this.number=0;
	this.birth="";
	this.zipcode="";
	this.location="";
	this.pref_zodiac=0;
	this.sex="";
	this.pref_sex="";
	this.marital=0;
	this.pref_marital=0;
	this.race=0;
	this.pref_race=0;
	this.hair=0;
	this.pref_hair=0;
	this.eyes=0;
	this.pref_eyes=0;
	this.weight=0;
	this.height=0;	/* in inches */
	this.min_weight=0;
	this.max_weight=0;
	this.min_height=0;
	this.max_height=0;
	this.min_age=0;
	this.max_age=0;
	this.min_zipcode="";
	this.max_zipcode="";
	this.income=0;
	this.min_income=0;
	this.max_income=0;
	this.created=0;
	this.updated=0;
	this.note=new Array("","","","","");
	this.misc=0;
	this.lastin=0;
	this.min_match=0;
	this.purity=0;
	this.mbtype="";
	this.photo=0;
	this.queans=new Array(new queans_t(), new queans_t(), new queans_t(), new queans_t(), new queans_t());
	this.sizeof=992;
	this.write=user_t_write;
	this.read=user_t_read;
	this.age=function() {
		var swapped=system.datestr(60*60*24*20).substr(0,2)!='01';
		var now=system.datestr();
		var nowyear=parseInt(now.substr(-2),10);
		var mmddnow=now.substr(0,5);
		var mmddbirth=this.birth.substr(0,5);
		var age;

		if(swapped) {
			mmddnow=mmddnow.substr(3)+mmddnow.substr(2,1)+mmddnow.substr(0,2);
			mmddbirth=mmddbirth.substr(3)+mmddbirth.substr(2,1)+mmddbirth.substr(0,2);
		}

		/* TODO: Max age of 105 and min age of 6 */
		if(nowyear < parseInt(this.birth.substr(-2), 10)+5)
			nowyear += 100;
		age=nowyear-parseInt(this.birth.substr(-2), 10);
		if(mmddnow >= mmddbirth)
			age++;
	};
	this.zodiac=function() {
		var bmon,bday;

		if(system.datestr(60*60*24*20).substr(0,2)!='01') {
			bmon=birth.substr(3,2);
			bday=parseInt(birth.substr(0,2), 10);
		}
		else {
			bday=parseInt(birth.substr(3,2), 10);
			bmon=birth.substr(0,2);
		}
		if((bmon=="03" && bday >= 21) || (bmon=="04" && bday <= 19)
			return(ZODIAC_ARIES);
		if((bmon=="04" && bday >= 20) || (bmon=="05" && bday <= 20)
			return(ZODIAC_TAURUS);
		if((bmon=="05" && bday >= 21) || (bmon=="06" && bday <= 20)
			return(ZODIAC_GEMINI);
		if((bmon=="06" && bday >= 21) || (bmon=="07" && bday <= 22)
			return(ZODIAC_CANCER);
		if((bmon=="07" && bday >= 23) || (bmon=="08" && bday <= 22)
			return(ZODIAC_LEO);
		if((bmon=="08" && bday >= 23) || (bmon=="09" && bday <= 22)
			return(ZODIAC_VIRGO);
		if((bmon=="09" && bday >= 23) || (bmon=="10" && bday <= 22)
			return(ZODIAC_LIBRA);
		if((bmon=="10" && bday >= 23) || (bmon=="11" && bday <= 21)
			return(ZODIAC_SCORPIO);
		if((bmon=="11" && bday >= 22) || (bmon=="12" && bday <= 21)
			return(ZODIAC_SAGITTARIUS);
		if((bmon=="12" && bday >= 22) || (bmon=="01" && bday <= 19)
			return(ZODIAC_CAPRICORN);
		if((bmon=="01" && bday >= 20) || (bmon=="02" && bday <= 18)
			return(ZODIAC_AQUARIUS);
		if((bmon=="02" && bday >= 19) || (bmon=="03" && bday <= 20)
			return(ZODIAC_PISCES);
		return(0xff);
	}
}

}

function wall_t_write(file)
{
	var str;

	str=this.name.toString().substr(0,25);
	while(str.length<26)
		str+=ascii(0);
	file.write(str);
	str=this.system.toString().substr(0,40);
	while(str.length<41)
		str+=ascii(0);
	file.write(str);
	str=this.text[0].toString().substr(0,70);
	while(str.length<71)
		str+=ascii(0);
	file.write(str);
	str=this.text[1].toString().substr(0,70);
	while(str.length<71)
		str+=ascii(0);
	file.write(str);
	str=this.text[2].toString().substr(0,70);
	while(str.length<71)
		str+=ascii(0);
	file.write(str);
	str=this.text[3].toString().substr(0,70);
	while(str.length<71)
		str+=ascii(0);
	file.write(str);
	str=this.text[4].toString().substr(0,70);
	while(str.length<71)
		str+=ascii(0);
	file.write(str);
	file.writeBin(written,4);
	file.writeBin(imported,4);
}

function wall_t()
{
	this.name="";
	this.system="";
	this.text=new Array("","","","","");
	this.written=0;
	this.imported=0;
	this.sizeof=430;
	this.write=wall_t_write;
	this.read=wall_t_read;
}

var TGRAM_NOTICE = "\1n\1h\1mNew telegram in \1wMatch Maker \1mfor you!" +
                "\7\1n\r\n";

function TM_YEAR(year)
{
	return(year%100);
}
