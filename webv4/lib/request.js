// Helpers for http_request
var request = {
	// Query parameter p exists
	hasParam: function (p) {
		return (Array.isArray(http_request.query[p]) && http_request.query[p].length);
	},
    // All query parameters in Array[String] p exist
	hasParams: function (p) {
		return p.every(function (e) {
			return (Array.isArray(http_request.query[e]) && http_request.query[e].length);
		});
	},
	// First instance of query parameter p, or undefined
	getParam: function (p) {
		if (this.hasParam(p)) return decodeURIComponent(http_request.query[p][0]);
	},
	getParamMulti: function (p) {
		return p.reduce((function (a, c) {
			if (this.hasParam(c)) a[c] = http_request.query[c].map(decodeURIComponent);
			return a;
		}).bind(this), {});
	},
	writeParam: function (p) {
		if (this.hasParam(p)) write(http_request.query[p][0]);
	},
	getCookie: function () {
		return http_request.cookie.synchronet;
	},
};

request;