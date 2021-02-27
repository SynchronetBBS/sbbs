// $Id: birthdate.js,v 1.4 2018/06/06 21:44:23 rswindell Exp $
/*
 * When this code is load()ed, User objects get an extra 'birthDate' property
 * which is a JavaScript Date object representing the user's birthdate.
 */

User.prototype.__defineGetter__("birthDate", function() {
	var mfirst=system.datestr(1728000).substr(0,2)=='01';
	var match;
	var m,d,y;

	match=this.birthdate.match(/^([0-9]{2})[-/]([0-9]{2})[-/]([0-9]{2})$/);
	if(match==null)
		return undefined;
	m=parseInt(match[mfirst?1:2],10)-1;
	d=parseInt(match[mfirst?2:1],10);
	y=parseInt(match[3],10)+1900;
	if(this.age + y < (new Date()).getYear()-2)
		y+=100;
	return new Date(y,m,d);
});
