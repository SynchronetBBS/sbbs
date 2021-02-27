File.prototype.iniGetBitField = function(section, k, bitmap, def) {
	var txt = this.iniGetValue(section, k, "");
	if (txt === "")
		txt = def;
	var ret = 0;
	var newmap = {};

	Object.keys(bitmap).forEach(function(k) {
		newmap[k.toLowerCase()] = bitmap[k];
	});

	if (typeof txt === 'string') {
		var fields = txt.split(/\s*\|\s*/);
		fields.forEach(function(f) {
			var i;
			var done = false;
			var lc = f.toLowerCase();

			if(newmap.hasOwnProperty(lc))
				ret |= newmap[lc];
			else {
				var i = parseInt(f);
				if (!isNaN(i))
					ret |= i;
			}
		});
	}
	else
		ret = def;
	return ret;
};

File.prototype.iniGetEnum = function(section, k, enum, def) {
	var txt = this.iniGetValue(section, k, "");

	if (txt === '')
		txt = def;

	// Trim spaces from both ends
	if (typeof txt == 'string') {
		txt = txt.replace(/^\s+|\s+$/gm, '').toLowerCase();
		for (i = 0; i < enum.length; i++) {
			if (enum[i].toLowerCase() === txt) {
				return i;
			}
		}
	}

	return parseInt(txt);
}

File.prototype.iniGetLogLevel = function(section, k, def) {
	return this.iniGetEnum(section, k, ["Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Informational", "Debugging"], def);
}
