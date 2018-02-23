// Based on p5-Net-ACME2 - https://github.com/FGasper/p5-Net-ACME2/

/*
 * var acme = new ACMEv2({key:CryptContext, key_id:undefined})
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
 * var order = acme.create_new_order({identifiers:[{type:"dns",value:"example.com"}]});
 * var authz = acme.get_authorization(order.authorizations[0]);
 * var challenges = authz.challenges;
 * 
 * // Pick a challenge, and satisfy it.
 * 
 * acme.accept_challenge(challenge);
 * 
 * while (!acme->poll_authorization(order.authorizations[0]))
 * 	mswait(1000);
 * 
 * // Make a key and CSR for *.example.com
 * 
 * order = acme.finalize_order(order, csr);
 * 
 * while (order.status !== 'valid') {
 * 	mswait(1000);
 * 	order = acme.poll_order(order);
 * }
 * 
 * var cert = acme.get_cert(order);
 */

require("http.js", "HTTPRequest");

function ACMEv2(opts)
{
	if (opts.key === undefined)
		throw('Need "key"!');

	this.key = opts.key;
	this.key_id = opts.key_id;
	if (opts.host !== undefined)
		this.host = opts.host;
	this.jws_format = 'sha256';
	this.ua = new HTTPRequest();
	if (this.key_id === undefined) {
		try {
			this.get_key_id();
		}
		catch(e) {}
	}
}

ACMEv2.prototype.get_key_id = function()
{
	this.create_new_account({termsOfServiceAgreed:true,onlyReturnExisting:true});
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

ACMEv2.prototype.FULL_JWT_METHODS = [
	'newAccount',
	'revokeCert'
];
ACMEv2.prototype.create_new_account = function(opts)
{
	return this.post('newAccount', opts);
}

ACMEv2.prototype.create_new_order = function(opts)
{
	var ret;

	if (opts.identifiers === undefined)
		throw("create_new_order() requires an identifier in opts");
	ret = JSON.parse(this.post('newOrder', opts));
	if (this.last_headers.Location !== undefined && this.last_headers.Location[0] !== undefined) {
		ret.Location=this.last_headers.Location[0];
		print("Location: "+ret.Location);
	}
	return ret;
}

ACMEv2.prototype.accept_challenge = function(challenge)
{
	var opts={keyAuthorization:challenge.token+"."+this.thumbprint()};

	return JSON.parse(this.post_url(challenge.url, opts));
}

ACMEv2.prototype.poll_authorization = function(auth)
{
	var ret = JSON.parse(this.ua.Get(auth));

	for (var challenge in ret.challenges) {
		if (ret.challenges[challenge].status == 'valid')
			return true;
	}
	return false;
}

ACMEv2.prototype.finalize_order = function(order, csr)
{
	var opts = {};

	if (order === undefined)
		throw("Missing order");
	if (csr === undefined)
		throw("Missing csr");
	if (typeof(csr) != 'object' || csr.export === undefined)
		throw("Invalid csr");
	opts.csr = this.base64url(csr.export(CryptCert.FORMAT.CERTIFICATE));
	return JSON.parse(this.post_url(order.finalize, opts));
}

ACMEv2.prototype.poll_order = function(order)
{
	var loc = order.Location;
	if (loc === undefined)
		throw("No order location!");
	var ret = JSON.parse(this.ua.Get(loc));
	ret.Location = loc;
	return ret;
}

ACMEv2.prototype.get_cert = function(order)
{
	if (order.certificate === undefined)
		throw("Order has no certificate!");
	var cert = this.ua.Get(order.certificate);
	return new CryptCert(cert);
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

ACMEv2.prototype.get_nonce = function()
{
	var ret;

	if (this.last_nonce === undefined) {
		ret = this.ua.Head(this.get_directory().newNonce);
		this.last_nonce = ret['Replay-Nonce'][0];
	}

	ret = this.last_nonce;
	this.last_nonce = undefined;
	return ret;
}

ACMEv2.prototype.get_authorization = function(url)
{
	return JSON.parse(this.ua.Get(url));
}

ACMEv2.prototype.base64url = function(string)
{
	string = base64_encode(string);
	string = string.replace(/=/g,'');
	string = string.replace(/\+/g,'-');
	string = string.replace(/\//g,'_');
	return string;
}

ACMEv2.prototype.hash_thing = function(data)
{
	var shactx;

	shactx = new CryptContext(CryptContext.ALGO.SHA2);
	shactx.encrypt(data);
	shactx.encrypt('');
	var MD = shactx.hashvalue;
	var D = '';
	var tmp;
	var i;

	D = ascii(0x30) + ascii(0x31) + ascii(0x30) + ascii(0x0d) + ascii(0x06) + ascii(0x09) +
		ascii(0x60) + ascii(0x86) + ascii(0x48) + ascii(0x01) + ascii(0x65) + ascii(0x03) +
		ascii(0x04) + ascii(0x02) + ascii(0x01) + ascii(0x05) + ascii(0x00) + ascii(0x04) +
		ascii(0x20) + MD;
	D = ascii(0) + D;
	while(D.length < this.key.keysize-2)
		D = ascii(255)+D;
	D = ascii(0x00) + ascii(0x01) + D;
	return this.key.decrypt(D);
}

// TODO: Should this be in http.js?
ACMEv2.prototype.store_headers = function(update_nonce)
{
	var h = {};

	if (update_nonce === undefined)
		update_nonce = false;
	for(i in this.ua.response_headers) {
		m = this.ua.response_headers[i].match(/^(.*?):\s*(.*?)\s*$/);
		if (m) {
			if (h[m[1]] == undefined)
				h[m[1]] = [];
			h[m[1]].push(m[2]);
		}
	}
	if (h['Replay-Nonce'] !== undefined) {
		if (update_nonce)
			this.last_nonce = h['Replay-Nonce'][0];
	}
	this.last_headers = h;
}

ACMEv2.prototype.thumbprint = function()
{
	var jwk = JSON.stringify({e:this.key.public_key.e, kty:'RSA', n:this.key.public_key.n});
	var shactx;

	shactx = new CryptContext(CryptContext.ALGO.SHA2);
	shactx.encrypt(jwk);
	shactx.encrypt('');
	var MD = shactx.hashvalue;
	return this.base64url(MD);
}

ACMEv2.prototype.post_url = function(url, data, post_method)
{
	var protected = {};
	var ret;
	var msg = {};

	if (post_method === undefined)
		post_method = 'post_key_id';

print("URL: "+url);
print("Method: "+post_method);
	switch(post_method) {
		case 'post_key_id':
			protected.nonce = this.get_nonce();
			protected.url = url;
			protected.alg = 'RS256';
			protected.kid = this.key_id;
			if (protected.kid === undefined)
				throw("No key_id available!");
			break;
		case 'post_full_jwt':
			protected.nonce = this.get_nonce();
			protected.url = url;
			protected.alg = 'RS256';
			protected.jwk = {e:this.key.public_key.e, kty:'RSA', n:this.key.public_key.n};
			break;
	}
	msg.protected = this.base64url(JSON.stringify(protected));
	msg.payload = this.base64url(JSON.stringify(data));
	msg.signature = this.base64url(this.hash_thing(msg.protected + "." + msg.payload));
	var body = JSON.stringify(msg);
	body = body.replace(/:"/g, ': "');
	body = body.replace(/:{/g, ': {');
	body = body.replace(/,"/g, ', "');
	ret = this.ua.Post(url, body);
	this.store_headers(true);
	if (this.last_headers['Location'] !== undefined && this.key_id === undefined)
		this.key_id = this.last_headers['Location'][0];
	return ret;
}
