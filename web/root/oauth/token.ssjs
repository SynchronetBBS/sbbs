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

//---------------

if (http_request.query['grant_type'] === undefined)
	die('No grant_type');
var gt = http_request.query['grant_type'][0];
if (gt != 'authorization_code')
	die('Bad grant_type');

if (http_request.query['code'] === undefined)
	die('No code');
var code = http_request.query['code'][0];
var cpad = rsa.encrypt(debase64url(code));
var m = cpad.match(/^([0-9]+):(.*?)(?::(.*?))?\x00+$/);
if (m === null)
	die('Invalid code');
if (parseInt(m[1]) > time() || parseInt(m[1]) < time() - 600)
	die('Invalid code');
var un = system.matchuser(m[2]);
if (un == 0)
	die('Invalid code');
var u = new User(un);
if (u == null || u.number == 0)
	die('Invalid code');
var nonce;
if (m[3] !== undefined && m[3].length > 0)
	nonce = m[3];

if (http_request.query['client_id'] === undefined)
	die('No client_id');
var client_id = http_request.query['client_id'][0];

var ru;
if (http_request.query['redirect_uri'] !== undefined)
	ru = http_request.query['redirect_uri'][0];
if (ru === undefined)
	die('No redirect');
// Fancy token generation here...
var tok = {};
// TODO: Store this somewhere/integrate with web server somehow
tok.access_token = makeid(16);
tok.token_type = 'Bearer';
tok.expires_in = 3600;
//tok.scope = '';
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Cache-Control'] = 'no-store';
http_reply.header['Pragma'] = 'no-cache';

var id = {};
if (rsa.algo == CryptContext.ALGO.RSA)
	id.alg = 'RS256';
else
	throw("Unsupported key");
id.kid = '1';
var claims = {};
claims.iss = 'https://'+system.host_name;
claims.sub = u.alias;
claims.aud = client_id;
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
tok.id_token = tid;

write(JSON.stringify(tok));
exit(0);

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

function debase64url(string)
{
	string = string.replace(/\-/g,'+');
	string = string.replace(/_/g,'/');
	string = base64_decode(string);
	while(string.length % 4)
		string += '=';
	return string;
};

function base64url(string)
{
	string = base64_encode(string);
	string = string.replace(/\=/g,'');
	string = string.replace(/\+/g,'-');
	string = string.replace(/\//g,'_');
	return string;
};

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

function makeid(len) {
	var text = "";
	var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	if (len === undefined)
		len = 5;

	for (var i = 0; i < len; i++)
		text += possible.charAt(Math.floor(Math.random() * possible.length));

	return text;
}
