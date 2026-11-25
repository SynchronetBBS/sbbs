function auth_rewrite() {
	if (http_request.request_string.search(/\/authorize\?/) != -1) {
		http_request.request_string = http_request.request_string.replace(/\/authorize\?/,'\/auth.ssjs?');
		return true;
	}
	return false;
}

function tok_rewrite() {
	if (http_request.request_string.search(/\/token$/) != -1) {
		http_request.request_string = http_request.request_string.replace(/\/token$/,'\/token.ssjs');
		return true;
	}
	return false;
}
