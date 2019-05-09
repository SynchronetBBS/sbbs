/*	Posts a notification message to a list of sub-boards when commits are made
	to a GitHub repository.

	- In your repository, go to Settings -> Webhooks -> Add webhook
	- Payload URL: https://your-bbs-hostname/api/github.ssjs
	- Content-type: application/json
	- Secret: some-secret-passphrase-here
	- Which events: Just the push event
	- Active: checked
	- Add webhook

	- On your BBS server, edit ctrl/modopts.ini
	- Add a [github-notify] section
	- For each repository you wish to configure, add a key-value pair like:
	- my-repository-name=secret,SUB
	- Where 'secret' is the secret passphrase as configured above
	- Where 'SUB' is the internal code of a sub-board to post to
	- Multiple subs can be specified, separated by commas
*/

load('sbbsdefs.js');
load('hmac.js');
const options = load({}, 'modopts.js', 'github_notify');
load(system.exec_dir + '../web/lib/init.js');

function b2h(str) {
	return str.split('').map(function (e) {
		var n = ascii(e).toString(16);
		return n.length < 2 ? ('0' + n) : n;
	}).join('');
}

function verify_signature(key, payload, hash) {
	return (b2h(hmac_sha1(key, payload)) === hash);
}

try {
	const hash = http_request.header['x-hub-signature'].split('=')[1];
	const payload = JSON.parse(http_request.post_data);
	if (typeof options[payload.repository.name] === 'undefined') {
		throw 'Unknown repository ' + payload.repository.name;
	}
	const subs = options[payload.repository.name].split(',');
	const secret = subs.shift();
	if (!verify_signature(secret, http_request.post_data, hash)) {
		throw 'GitHub signature mismatch';
	}
} catch (err) {
	log(LOG_ERR, err);
	exit();
}

const header = {
	from: payload.head_commit.author.username,
	to: 'All',
	subject: 'Changes to ' + payload.repository.full_name
};

const body = payload.commits.map(function (e) {
	const ret = [ 'Commit ID: ' + e.id, 'Author: ' + e.author.username ];
	if (e.added.length) ret.push('Added: ' + e.added.join(', '));
	if (e.removed.length) ret.push('Removed: ' + e.removed.join(', '));
	if (e.modified.length) ret.push('Modified: ' + e.modified.join(', '));
	ret.push('', 'Message:', e.message, '', 'Commit URL:', e.url, '');
	return ret.join('\r\n');
}).concat('Repository URL: ' + payload.repository.url).join('\r\n\r\n');

subs.forEach(function (sub) {
	try {
		const mb = new MsgBase(sub);
		if (!mb.open()) throw 'Failed to open message base ' + sub;
		if (!mb.save_msg(header, body)) throw 'Failed to save message';
		mb.close();
	} catch (err) {
		log(LOG_ERR, err);
	}
});

const _tf = new File(system.temp_dir + 'github_notify');
if (_tf.open('w')) {
    _tf.write(body);
    _tf.close();
}
