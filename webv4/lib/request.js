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
	// Return an array of all values supplied for query param 'p'
	// eg. ?a=a&a=b&a=c&a=d would yield [ 'a', 'b', 'c', 'd' ]
	getParamMulti: function (p) {
		if (this.hasParam(p)) return http_request.query[p].map(decodeURIComponent);
		return [];
	},
	// As above but returns an object with param names as keys, with array properties
	// eg. ?a=a&b=c&a=b&b=d would yield { a: [ 'a', 'b' ], b: [ 'c', 'd' ] }
	getParamsMulti: function (p) {
		return p.reduce((function (a, c) {
			if (this.hasParam(c)) {
				a[c] = http_request.query[c].map(decodeURIComponent);
			} else {
				a[c] = [];
			}
			return a;
		}).bind(this), {});
	},
	// Print the value of parameter 'p' if it exists, otherwise do nothing
	writeParam: function (p) {
		if (this.hasParam(p)) write(this.getParam(p));
	},
	getCookie: function () {
		return http_request.cookie.synchronet;
	},
};

request;