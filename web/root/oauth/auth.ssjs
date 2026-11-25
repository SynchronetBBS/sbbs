var ks_fname = backslash(system.ctrl_dir)+"oid.key";
var maincnf_fname = backslash(system.ctrl_dir)+"main.cnf";
var rsa;
var syspass;
var maincnf = new File(maincnf_fname);
var keysize = 256;	// TODO: Make configurable... ECC sizes are 32, 48, and 66

if (!maincnf.open("rb", true))
	throw("Unable to open "+maincnf.name);
maincnf.position = 186; // Indeed.
syspass = maincnf.read(40);
syspass = syspass.replace(/\x00/g,'');
maincnf.close();

ks = new CryptKeyset(ks_fname, file_exists(ks_fname) ? CryptKeyset.KEYOPT.NONE : CryptKeyset.KEYOPT.CREATE);

try {
	rsa = ks.get_private_key(system.host_name, syspass);
}
catch(e2) {
	rsa = new CryptContext(CryptContext.ALGO.RSA);
	rsa.keysize=keysize;
	rsa.label=system.host_name;
	rsa.generate_key();
	ks.add_private_key(rsa, syspass);
}

//--------------

if (http_request.query['response_type'] === undefined)
	die('No response_type');
var rt = http_request.query['response_type'][0];
if (rt != 'code' && rt != 'id_token' && rt != 'id_token token' && rt != 'token')
	die('Bad response_type');

if (http_request.query['client_id'] === undefined)
	die('No client_id');
var cid = http_request.query['client_id'][0];

var ru;
if (http_request.query['redirect_uri'] !== undefined)
	ru = http_request.query['redirect_uri'][0];

var scope;
if (http_request.query['scope'] !== undefined)
	scope = http_request.query['scope'][0];

var state;
if (http_request.query['state'] !== undefined)
	state = http_request.query['state'][0];

var nonce;
if (http_request.query['nonce'] !== undefined)
	nonce = http_request.query['nonce'][0];

if (ru === undefined)
	die('No redirect');

switch (rt) {
case 'code':
	// Generate a fancy auth code here...
	var code;
	code = time()+":"+user.alias
	if (nonce !== undefined)
		code += ':'+nonce;
	var cpad = code;
	while (cpad.length % (rsa.keysize))
		cpad += ascii(0);
	code = base64url(rsa.decrypt(cpad));

	if (ru === undefined)
		die('No redirect');

	append_ru('?', 'code', code);
	append_ru('?', 'state', state);
	break;
case 'id_token':
	append_ru('#', 'expires_in', 3600);
	append_ru('#', 'state', state);
	append_ru('#', 'id_token', make_id_token());
	break;
case 'id_token token':
	append_ru('#', 'access_token', makeid(16));
	append_ru('#', 'token_type', 'Bearer');
	append_ru('#', 'expires_in', 3600);
	append_ru('#', 'state', state);
	append_ru('#', 'scope', scope);
	append_ru('#', 'id_token', make_id_token());
	break;
case 'token':
	append_ru('#', 'access_token', makeid(16));
	append_ru('#', 'token_type', 'Bearer');
	append_ru('#', 'expires_in', 3600);
	append_ru('#', 'state', state);
	append_ru('#', 'scope', scope);
	break;
default:
	die();
}

http_reply.status = '302 Success';
http_reply.header['Location'] = ru;
log("Redirecting to "+ru);
exit(0);

function append_ru(sep, key, value)
{
	if (value === undefined)
		return;
	if (ru.indexOf(sep) == -1)
		ru += sep;
	else
		ru += '&';
	ru += encodeURIComponent(key)+'='+encodeURIComponent(value);
}

function die(reason)
{
	if (reason === undefined)
		reason = 'You suck';
	http_reply.status = '400 '+reason;
	write(reason);
	exit(0);
}

function error(reason)
{
	var r = ru;

	if (ru === undefined)
		die('No redirect');
	if (r.indexOf('?') == -1)
		r += '?';
	else
		r += '&';

	r += 'error='+encodeURIComponent(reason);
	if (state !== undefined) {
		r += '&state=';
		r += encodeURIComponent(state);
	}

	http_reply.status = '302 Error';
	http_reply.header['Location'] = r;
	exit(0);
}

function base64url(string)
{
	string = base64_encode(string);
	string = string.replace(/\=/g,'');
	string = string.replace(/\+/g,'-');
	string = string.replace(/\//g,'_');
	return string;
};

function makeid(len) {
	var text = "";
	var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	if (len === undefined)
		len = 5;

	for (var i = 0; i < len; i++)
		text += possible.charAt(Math.floor(Math.random() * possible.length));

	return text;
}

function make_id_token()
{
	var id = {};
	if (rsa.algo == CryptContext.ALGO.RSA)
		id.alg = 'RS256';
	else
		throw("Unsupported key");
	id.kid = '1';
	var claims = {};
	claims.iss = 'https://'+system.host_name;
	claims.sub = user.alias;
	claims.aud = cid;
	claims.exp = time() + 3600;
	claims.iat = time();
	if (nonce !== undefined)
		claims.nonce = nonce;
	claims.auth_time = claims.iat;
	var tid = base64url(JSON.stringify(id))+'.'+base64url(JSON.stringify(claims));

	shactx = new CryptContext(CryptContext.ALGO.SHA2);
	shactx.blocksize = 256/8;
	shactx.encrypt(tid);
	shactx.encrypt('');

	tid += '.' + base64url(rsa_hash(shactx, rsa));
	return tid;
}

function rsa_hash(shactx, key) {
	var MD = shactx.hashvalue;
	var D = '';

	D = ascii(0x30) + ascii(0x31) + ascii(0x30) + ascii(0x0d) + ascii(0x06) + ascii(0x09) +
		ascii(0x60) + ascii(0x86) + ascii(0x48) + ascii(0x01) + ascii(0x65) + ascii(0x03) +
		ascii(0x04) + ascii(0x02) + ascii(0x01) + ascii(0x05) + ascii(0x00) + ascii(0x04) +
		ascii(0x20) + MD;
	D = ascii(0) + D;
	while(D.length < key.keysize-2)
		D = ascii(255)+D;
	D = ascii(0x00) + ascii(0x01) + D;
	return key.decrypt(D);
}

