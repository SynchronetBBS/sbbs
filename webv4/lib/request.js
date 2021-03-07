// Helpers for http_request
var request = {
	// Query parameter p exists
	has_param: function (p) {
		return (Array.isArray(http_request.query[p]) && http_request.query[p].length);
	},
    // All query parameters in Array[String] p exist
	has_params: function (p) {
		return p.every(function (e) {
			return (Array.isArray(http_request.query[e]) && http_request.query[e].length);
		});
	},
	// First instance of query parameter p, or undefined
	get_param: function (p) {
		if (Array.isArray(http_request.query[p]) && http_request.query[p].length) {
			return http_request.query[p][0];
		}
	},
	write_param: function (p) {
		if (Array.isArray(http_request.query[p]) && http_request.query[p].length) {
			write(http_request.query[p][0]);
		}
	}
};

request;