// Based on p5-Net-ACME2 - https://github.com/FGasper/p5-Net-ACME2/

/*
 * var acme = new ACMEv2({key:key, key_id:undefined})
 * 
 * // For a new account:
 * {
 * 	var terms_url = acme.get_terms_of_service();
 * 
 * 	acme->create_new_account({termsOfServiceAgreed:1});
 * }
 * 
 * // Save the key_id somewhere so it can be reused.
 * 
 * var order = acme->create_new_order({identifiers:[{type:'dns',value:'*.example.com'}]);
 * var authz = acme->get_authorization(order->authorizations()[0]);
 * var challenges = authz->challenges();
 * 
 * // Pick a challenge, and satisfy it.
 * 
 * acme->accept_challenge(challenge);
 * 
 * while (!acme->poll_authorization(authz))
 * 	mswait(1000);
 * 
 * // Make a key and CSR for *.example.com
 * 
 * acme->finalize_order(order, csr);
 * 
 * while (order.status !== 'valid') {
 * 	mswait(1000);
 * 	acme->poll_order(order);
 * }
 * 
 * var certificate_url = order->certificate();
 */

require("http.js", "HTTPRequest");

function ACMEv2(opts)
{
	if (opts.key === undefined)
		throw('Need "key"!');

	this.key = opts.key;
	if (opts.key_id !== undefined)
		this.key_id = opts.key_id;
	if (opts.host !== undefined)
		this.host = opts.host;
	this.jws_format = 'sha256';
	this.ua = new HTTPRequest();
}

ACMEv2.prototype.host = "acme-staging-v02.api.letsencrypt.org";
ACMEv2.prototype.dir_path = "/directory";

ACMEv2.prototype.get_terms_of_service = function()
{
	var dir = this.get_directory();
	if (dir.meta === undefined)
		throw('No "meta" in directory!');
	if (dir.meta.termsOfService === undefined)
		throw('No "termsOfService" in directory metadata');
	return dir.meta.termsOfService;
}

ACMEv2.prototype.get_directory = function()
{
	if (this.directory === undefined) {
		this.directory = JSON.parse(this.ua.Get("https://"+this.host+this.dir_path));
	}
	return this.directory;
}

const ACMEv2.prototype.FULL_JWT_METHODS = [
	'newAccount',
	'revokeCert'
];
ACMEv2.prototype.create_new_account = function(opts)
{
	
}

ACMEv2.prototype.post = function(link, data)
{
	var post_method;
	var url;

	if (this.FULL_JWT_METHODS.indexOf(link) > -1)
		post_method = 'post_full_jwt';
	url = this.get_directory(link)[link];
	if (url === undefined)
		throw('Unknown link name: "'+link+'"');
	return this.post_url(url, data, post_method);
}

ACMEv2.prototype.post_url = function(url, data, post_method)
{
	if (post_method === undefined)
		post_method = 'post_key_id';

	switch(post_method) {
		case 'post_key_id':
			break;
		case 'post_full_jwt':
			
			break;
	}
	return this.post_method(url, data);
}

ACMEv2.prototype.post_method = function(url, data, post_method)
{
}
