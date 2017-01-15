load('oauth.js');

var Twitter = function (key, secret, token, token_secret) {

	var self = this;

	this.api_url = 'https://api.twitter.com/1.1';
	this.key = key;
	this.secret = secret;
	this.token = token;
	this.token_secret = token_secret;

	var endpoints = {
		search : {
			tweets : {
				method : 'search',
				required : { q : '' },
				http_method : 'get'
			}
		},
		statuses : {
			update : {
				method : 'tweet',
				required : { status : '' },
				http_method : 'post'
			},
			user_timeline : {
				method : 'get_user_timeline',
				required : { screen_name : '' },
				http_method : 'get'
			}
		}
	};

	/*	Send a signed OAuth1 POST request to this.api_url + 'path'.
		POST data will be constructed from key/value pairs in 'obj'. */
	this.post = function (path, obj) {
		return JSON.parse(
			(new OAuth1_Client()).post(
				this.api_url + path, obj,
				this.key, this.secret, this.token, this.token_secret
			)
		);
	}

	/*	Send a signed OAuth1 GET request to this.api_url + 'path'.
		Query parameters will be constructed from key/value pairs in 'obj'.	*/
	this.get = function (path, obj) {
		return JSON.parse(
			(new OAuth1_Client()).get(
				this.api_url + path + '?' + param_stringify(obj, false, '&'),
				this.key, this.secret, this.token, this.token_secret
			)
		);
	}

	// Populate methods from described REST API endpoints
	function methodist(obj, path) {
		for (var property in obj) {
			if (typeof obj[property].method === 'undefined') {
				methodist(obj[property], path + property + '/');
			} else {
				 (function (obj, property, path) {
				 	self[obj[property].method] = function (data) {
						if (typeof data === 'undefined') var data = {};
						if (typeof obj[property].required === 'object') {
							for (var r in obj[property].required) {
								if (typeof data[r] !==
									typeof obj[property].required[r]
								) {
									throw obj[property].method + ': missing ' + r;
								}
							}
						}
						return self[obj[property].http_method](
							path + property + '.json', data
						);
					}
				})(obj, property, path);
			}
		}
	}
	methodist(endpoints, '/');

}