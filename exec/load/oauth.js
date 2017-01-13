load('hmac.js');
load('http.js');

function _encodeURIComponent(str) {
	return encodeURIComponent(str).replace(
  		/[!'()*]/g, function(c) { return '%' + c.charCodeAt(0).toString(16); }
  	);
}

function generate_nonce(l) {
	var s = '';
	var c = '0123456789ABCDEFabcdef'.split('');
	for (var n = 0; n < l; n++) s += c[Math.floor(Math.random() * c.length)];
	return s;
}

function merge_objects() {
	var o = {};
	Array.prototype.slice.call(arguments).forEach(
		function (e) { Object.keys(e).forEach(function (f) { o[f] = e[f]; }); }
	);
	return o;
}

function param_stringify(params, quote, delim) {
	return Object.keys(params).map(
		function (e) {
			return (
				_encodeURIComponent(e) + '=' +
				(quote ? '"' : '') +
				_encodeURIComponent(params[e]) +
				(quote ? '"' : '')
			);
		}
	).sort().join(delim);
}

var OAuth1_Client = function () {

	function build_oauth_params(consumer_key, token) {
		var obj = {
			oauth_consumer_key : consumer_key,
			oauth_nonce : generate_nonce(32),
			oauth_signature_method : 'HMAC-SHA1',
			oauth_timestamp : time() + '',
			oauth_version : '1.0'
		};
		if (token) obj.oauth_token = token;
		return obj;
	}

	function get_base_string(method, url, params, data) {
		if (data) params = merge_objects(params, data);
		return (
			_encodeURIComponent(method) + '&' +
			_encodeURIComponent(url) + '&' +
			_encodeURIComponent(param_stringify(params, false, '&'))
		);
	}

	function get_signature(base_string, consumer_secret, token_secret) {
		return base64_encode(
			hmac_sha1(
				_encodeURIComponent(consumer_secret) +
				'&' +
				(token_secret ? _encodeURIComponent(token_secret) : ''),
				base_string
			)
		);
	}

	this.get = function (url, key, secret, token, token_secret) {
		url = url.split('?');
		var _url = url.shift();
		var data = {};
		if (url.length > 0) {
			url.join('?').split('&').forEach(
				function (e) {
					e = e.split('=');
					if (e.length < 2) return;
					data[e[0]] = [e][1];
				}
			);
		}
		var params = build_oauth_params(key, token);
		var base_string = get_base_string('GET', _url, params, data);
		params.oauth_signature = get_signature(base_string, secret, token_secret);
		return (new HTTPRequest(
			false, false,
			{ Authorization : 'OAuth ' + param_stringify(params, true, ', ') }
		)).Get(_url + '?' + param_stringify(data, false, '&'));
	}

	this.post = function (url, data, key, secret, token, token_secret) {
		var params = build_oauth_params(key, token);
		var base_string = get_base_string('POST', url, params, data);
		params.oauth_signature = get_signature(base_string, secret, token_secret);
		return (new HTTPRequest(
			false, false,
			{ Authorization : 'OAuth ' + param_stringify(params, true, ', ') }
		)).Post(url, param_stringify(data, false, '&'));
	}

}