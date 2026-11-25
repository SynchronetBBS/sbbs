// Finger and WebFinger use the same public profile visibility settings.
(function () {
	var options = load("modopts.js", "fingerservice") || {};
	["include_age", "include_gender", "include_location", "include_real_name"].forEach(function (key) {
		if (options[key] === undefined)
			options[key] = true;
	});
	return options;
})();
