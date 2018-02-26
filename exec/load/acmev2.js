// Inspired by p5-Net-ACME2 - https://github.com/FGasper/p5-Net-ACME2/

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
	this.ua = new HTTPRequest();
	try {
		this.get_key_id();
	}
	catch(e) {}
}

ACMEv2.prototype.get_key_id = function()
{
	if (this.key_id === undefined)
		return(this.create_new_account({onlyReturnExisting:true}));
};

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
};

ACMEv2.prototype.get_directory = function()
{
	if (this.directory === undefined) {
		var ret = this.ua.Get("https://"+this.host+this.dir_path);
		if (this.ua.response_code != 200) {
			log(LOG_DEBUG, ret);
			throw("Error fetching directory");
		}
		this.update_nonce();
		this.directory = JSON.parse(ret);
	}
	return this.directory;
};

ACMEv2.prototype.create_new_account = function(opts)
{
	var ret = this.post('newAccount', opts);

	if (this.ua.response_code != 201 && this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("newAccount returned "+this.ua.response_code+", not a 200 or 201 status!");
	}

	if (this.ua.response_headers_parsed.Location === undefined) {
		log(LOG_DEBUG, this.ua.response_headers.join("\n"));
		throw("No Location header in newAccount response.");
	}
	this.key_id = this.ua.response_headers_parsed.Location[0];
	return JSON.parse(ret);
};

ACMEv2.prototype.update_account = function(opts)
{
	this.get_key_id();
	var ret = this.post_url(this.key_id, opts);

	if (this.ua.response_code != 201 && this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("update_account returned "+this.ua.response_code+", not a 200 or 201 status!");
	}
	return JSON.parse(ret);
};

ACMEv2.prototype.get_account = function()
{
	return this.update_account({});
};

ACMEv2.prototype.create_new_order = function(opts)
{
	var ret;

	if (opts.identifiers === undefined)
		throw("create_new_order() requires an identifier in opts");
	ret = this.post('newOrder', opts);
	if (this.ua.response_code != 201) {
		log(LOG_DEBUG, ret);
		throw("newOrder responded with "+this.ua.response_code+" not 201");
	}
	ret = JSON.parse(ret);

	if (this.ua.response_headers_parsed.Location === undefined) {
		log(LOG_DEBUG, this.ua.response_headers.join("\n"));
		throw("No Location header in 201 response.");
	}
	ret.Location=this.ua.response_headers_parsed.Location[0];

	return ret;
};

ACMEv2.prototype.accept_challenge = function(challenge)
{
	var opts={keyAuthorization:challenge.token+"."+this.thumbprint()};
	var ret = this.post_url(challenge.url, opts);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("accept_challenge did not return 200");
	}
	return JSON.parse(ret);
};

ACMEv2.prototype.poll_authorization = function(auth)
{
	var ret = JSON.parse(this.ua.Get(auth));

	if (this.ua.response_code != 200)
		return false;
	this.update_nonce();

	for (var challenge in ret.challenges) {
		if (ret.challenges[challenge].status == 'valid')
			return true;
		if (ret.challenges[challenge].status == 'invalid') {
			log(LOG_DEBUG, JSON.stringify(ret.challenges[challenge]));
			throw ("Authorization failed... "+auth);
		}
	}
	return false;
};

ACMEv2.prototype.finalize_order = function(order, csr)
{
	var opts = {};

	if (order === undefined)
		throw("Missing order");
	if (csr === undefined)
		throw("Missing csr");
	if (typeof(csr) != 'object' || csr.export_cert === undefined)
		throw("Invalid csr");
	opts.csr = this.base64url(csr.export_cert(CryptCert.FORMAT.CERTIFICATE));

	var ret = this.post_url(order.finalize, opts);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("finalize_order did not return 200");
	}

	return JSON.parse(ret);
};

ACMEv2.prototype.poll_order = function(order)
{
	var loc = order.Location;
	if (loc === undefined)
		throw("No order location!");
	var ret = this.ua.Get(loc);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("order poll did not return 200");
	}
	this.update_nonce();

	ret = JSON.parse(ret);
	ret.Location = loc;
	return ret;
};

ACMEv2.prototype.asn1_len = function (len) {
	var ret = '';
	var tmplen;

	if (len < 128)
		return ascii(len);
	tmplen = len;
	var count = 0;
	while (tmplen) {
		ret = ascii(tmplen & 0xff)+ret;
		tmplen >>= 8;
		count++;
	}
	ret = ascii(0x80 | count) + ret;
	return ret;
};

ACMEv2.prototype.change_key = function(new_key)
{
	var inner = {protected:{}, payload:{}};
	var old_key = this.key;

	if (new_key === undefined)
		throw("change_key() requires a new key.");
	/* Create the inner object signed with old key */
	inner.protected.alg = 'RS256';
	inner.protected.jwk = {e:new_key.public_key.e, kty:'RSA', n:new_key.public_key.n};
	inner.protected.url = this.get_directory().keyChange;
	inner.payload.account = this.key_id;
	inner.payload.newKey = inner.protected.jwk;
	inner.protected = this.base64url(JSON.stringify(inner.protected));
	inner.payload = this.base64url(JSON.stringify(inner.payload));
	this.key = new_key;
	inner.signature = this.base64url(this.hash_thing(inner.protected + "." + inner.payload));
	this.key = old_key;
	ret = this.post('keyChange', inner);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("keyChange did not return 200");
	}
	this.key = new_key;
	return JSON.parse(ret);
}

ACMEv2.prototype.create_pkcs7 = function(cert)
{
	var ret = '';
	var certs = [];
	var m;
	var onecert;
	var rex = /(-+BEGIN[^\-]+-+[^\-]+-+END[^\-]+-+\n)/g;

	while ((m = rex.exec(cert)) != null) {
		onecert = new CryptCert(m[1]);
		ret = ret + onecert.export_cert(CryptCert.FORMAT.CERTIFICATE);
	}
	ret = this.asn1_len(ret.length) + ret;
	ret = ascii(0xa0) + ret;
	ret = ret + ascii(0x31) + ascii(0x00);
	ret = ascii(0x02) + ascii(0x01) + ascii(0x01) + ascii(0x31) + ascii(0x00) + ascii(0x30) + ascii(0x0B) + ascii(0x06) + ascii(0x09) + ascii(0x2A) + ascii(0x86) + ascii(0x48) + ascii(0x86) + ascii(0xF7) + ascii(0x0D) + ascii(0x01) + ascii(0x07) + ascii(0x01) + ret;
	ret = this.asn1_len(ret.length) + ret;
	ret = ascii(0x30) + ret;
	ret = this.asn1_len(ret.length) + ret;
	ret = ascii(0xa0) + ret;
	ret = ascii(0x06) + ascii(0x09) + ascii(0x2A) + ascii(0x86) + ascii(0x48) + ascii(0x86) + ascii(0xF7) + ascii(0x0D) + ascii(0x01) + ascii(0x07) + ascii(0x02) + ret;
	ret = this.asn1_len(ret.length) + ret;
	ret = ascii(0x30) + ret;

	return ret;
};

ACMEv2.prototype.get_cert = function(order)
{
	if (order.certificate === undefined)
		throw("Order has no certificate!");
	var cert = this.ua.Get(order.certificate);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, cert);
		throw("get_cert request did not return 200");
	}
	this.update_nonce();

	return new CryptCert(this.create_pkcs7(cert));
};

ACMEv2.prototype.FULL_JWT_METHODS = [
	'newAccount',
	'revokeCert'
];
ACMEv2.prototype.post = function(link, data)
{
	var post_method;
	var url;

	if (this.FULL_JWT_METHODS.indexOf(link) > -1)
		post_method = 'post_full_jwt';
	url = this.get_directory()[link];
	if (url === undefined)
		throw('Unknown link name: "'+link+'"');
	return this.post_url(url, data, post_method);
};

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
};

ACMEv2.prototype.get_authorization = function(url)
{
	var ret = this.ua.Get(url);

	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("get_authorization request did not return 200");
	}
	this.update_nonce();

	return JSON.parse(ret);
};

ACMEv2.prototype.base64url = function(string)
{
	string = base64_encode(string);
	string = string.replace(/\=/g,'');
	string = string.replace(/\+/g,'-');
	string = string.replace(/\//g,'_');
	return string;
};

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
};

ACMEv2.prototype.update_nonce = function()
{
	if (this.ua.response_headers_parsed['Replay-Nonce'] !== undefined)
		this.last_nonce = this.ua.response_headers_parsed['Replay-Nonce'][0];
};

ACMEv2.prototype.thumbprint = function()
{
	var jwk = JSON.stringify({e:this.key.public_key.e, kty:'RSA', n:this.key.public_key.n});
	var shactx;

	shactx = new CryptContext(CryptContext.ALGO.SHA2);
	shactx.encrypt(jwk);
	shactx.encrypt('');
	var MD = shactx.hashvalue;
	return this.base64url(MD);
};

ACMEv2.prototype.post_url = function(url, data, post_method)
{
	var protected = {};
	var ret;
	var msg = {};

	if (post_method === undefined)
		post_method = 'post_key_id';

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
	body = body.replace(/:\{/g, ': {');
	body = body.replace(/,"/g, ', "');
	ret = this.ua.Post(url, body, undefined, undefined, "application/jose+json");
	/* We leave error handling to the caller */
	if (this.ua.response_code == 200 || this.ua.response_code == 201)
		this.update_nonce();
	return ret;
};

// TODO: keyChange
// TODO: revokeCert
// TODO: Deactivate an Authorization
