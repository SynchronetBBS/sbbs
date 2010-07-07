Bot_Commands["WEATHER"] = new Bot_Command(0,false,false);
Bot_Commands["WEATHER"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!cmd[1])
		cmd[1] = onick;

	var query = "";

	var usr = new User(system.matchuser(cmd[1]));
	cmd.shift();
	if (typeof(usr)=='object')
		query = usr.location;
	if (!query)
		query = cmd.join(' ');
	query = query.replace(/[ ]/,'%20');

	var weather_url = "http://api.wunderground.com/auto/wui/geo/WXCurrentObXML/index.xml?query=" + query;

	var Weather = new XML((new HTTPRequest().Get(weather_url)).replace(/<\?.*\?>[\r\n\s]*/,''));

	var str = Weather.display_location.full + " - "
				+ Weather.temperature_string
				+ " (Observed at: " + Weather.observation_location.city
				+ ")";
/*	if (Weather.display_location.full=="" || !Weather.temperature_string=="")
		str = "No result."; */

	srv.o(target, str);
	return true;
}
