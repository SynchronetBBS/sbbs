load('sbbsdefs.js');
load(js.exec_dir + 'twitter.js');
var options = load({}, 'modopts.js', 'twitter');

if (argv.length < 1) exit();

try {
	(new Twitter(
		options.consumer_key, options.consumer_secret,
		options.access_token, options.access_token_secret
	)).tweet({ status : argv.join(' ') });
} catch (err) {
	log(LOG_ERR, err);
}