require('sbbsdefs.js', 'LEN_ALIAS');
var month = argv[0];
var day = argv[1];
if(month === undefined)
	month = new Date().getMonth();
var list = load({}, "birthdays.js", month, day);
var monthNameList = [ "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" ];
if(js.global.console) console.attributes = HIGH;
print(format("%u %s users with birthdays in %s:", list.length, system.name, monthNameList[month]));
for(var i = 0; i < list.length; i++) {
	if(js.global.console) console.attributes = HIGH | ((i&1) ? CYAN : MAGENTA);
	var u = User(list[i]);
	var str = format("%-*s ", LEN_ALIAS, u.alias);
	if((js.global.console && console.current_column + str.length > console.screen_columns) || (!js.global.console && i > 0 && i % 3 == 0))
		print();
	write(str);
}
print();
