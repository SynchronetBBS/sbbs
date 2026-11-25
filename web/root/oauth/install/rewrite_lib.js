function wf_rewrite()
{
	if (http_request.request_string.search(/\/webfinger/) != -1) {
		http_request.request_string = http_request.request_string.replace(/\/webfinger/,'\/webfinger.ssjs');
		return true;
	}
	return false;
}

function oid_conf_rewrite()
{
	if (http_request.request_string.search(/\/openid-configuration/) != -1) {
		http_request.request_string = http_request.request_string.replace(/\/openid-configuration/,'\/openid-configuration.ssjs');
		return true;
	}
	return false;
}
