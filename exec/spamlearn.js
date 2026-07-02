// spamlearn.js

// SpamAssassin Bayes trainer for Synchronet
// For use as a mailproc.ini script: feeds (redirected) messages to a
// running/listening spamd to train its Bayes classifier as spam or ham.

// spamd must be started with --allow-tell for the TELL (learn) command to work.

// IMPORTANT: forward messages to the learn address by *redirecting* (a.k.a.
// resend/bounce), which preserves the original message intact.  A plain inline
// "forward" rewrites the headers and body and trains Bayes on your forward
// wrapper instead of the original spam/ham - nearly useless.

// ---------------------------------------------------------------------------
// Example mailproc.ini entries (gate to the authenticated sysop so outsiders
// cannot poison your Bayes database by mailing the learn addresses):
//
// [spamlearn.js spam]
// 	to = spamlearn
// 	passthru = false
// 	AccessRequirements = user equal 1
// 	ProcessDNSBL = true
// 	ProcessSpam = true
//
// [spamlearn.js ham]
// 	to = hamlearn
// 	passthru = false
// 	AccessRequirements = user equal 1
// 	ProcessDNSBL = true
// 	ProcessSpam = true
//
// passthru = false causes the message to be consumed (accepted with an SMTP
// success reply, then dropped) - it is learned, not delivered or bounced.
// ---------------------------------------------------------------------------

// Honeypot / spam-trap use: give a bait address a section with "block" so each
// hit both trains Bayes as spam AND filters the sender's IP (via the same
// filter_ip mechanism Synchronet's built-in spam-bait uses - honoring
// spamblock_exempt.cfg and de-duplicating).  Such a section replaces a
// spambait.cfg entry (which would otherwise short-circuit at RCPT, before any
// mail processor runs) and should be listed BEFORE the scanning [SPAMC] section
// so trap mail is learned/blocked rather than rejected-without-learning:
//
// [spamlearn.js spam block dest 192.168.1.2]
// 	to = jseng@synchro.net, stats@*, spam@*
// 	passthru = false
// 	ProcessDNSBL = true
// 	ProcessSpam = true

// Options:
// spam | ham       message class to learn (default: spam)
// block [seconds]  after learning, filter the sender's IP for [seconds]
//                  (default 180 days) - for honeypot/spam-trap addresses
// dest <ip_address>
// port <tcp_port>
// username <user>  (default: none -> spamd's own run-as Bayes database)
// max-size <bytes>
// debug

load('sockdefs.js');
load('salib.js');

function main()
{
	var address = '127.0.0.1';
	var tcp_port = 783;
	var user;
	var msgclass = 'spam';
	var max_size = 500000;	/* bytes */
	var debug = false;
	var block = false;
	var block_duration = 60*60*24*180;	/* seconds (180 days), matches spam-bait default */

	// Process arguments:
	for(i=0; i<argc; i++) {

		// Strip any pre-pended slashes
		while(argv[i].charAt(0)=='-')
			argv[i]=argv[i].slice(1);

		if(argv[i]=='spam' || argv[i]=='ham')
			msgclass = argv[i];
		else if(argv[i]=='block') {
			block = true;
			var bd;
			if(!isNaN(bd = parseFloat(argv[i+1])))	/* optional duration */
				block_duration = bd, i++;
		}
		else if(argv[i]=='d' || argv[i]=='dest')
			address = argv[++i]; // Note: only one address supported (unlike spamc)
		else if(argv[i]=='p' || argv[i]=='port')
			tcp_port = Number(argv[++i]);
		else if(argv[i]=='u' || argv[i]=='username')
			user = argv[++i];
		else if(argv[i]=='s' || argv[i]=='max-size')
			max_size = Number(argv[++i]);
		else if(argv[i]=='debug')
			debug = true;
	}

	if(max_size && file_size(message_text_filename) > max_size) {
		log(LOG_INFO,"message size > max_size (" + max_size + ")");
		return;		// consumed by passthru=false; nothing learned
	}

	var msg=new SPAMC_Message(message_text_filename, address, tcp_port, user);
	if(msg.error != undefined) {
		log(LOG_ERR,"!ERROR "+msg.error);
		return;
	}
	msg.debug = debug;

	log(LOG_INFO, "learning message as " + msgclass.toUpperCase()
		+ " with SPAMD at " + address + " port " + tcp_port);
	var ret=msg.learn(msgclass);
	if(ret.error != undefined)			/* still block below on a trap hit */
		log(LOG_ERR,"!ERROR "+ret.error);
	else if(ret.learned)
		log(LOG_INFO, "learned message as " + msgclass.toUpperCase());
	else
		log(LOG_INFO, "message already known to Bayes as " + msgclass.toUpperCase());

	/* Honeypot/spam-trap mode: filter the sender's IP.  A trap hit is spam by
	   definition, so block regardless of whether Bayes just learned it.  Uses
	   the same filter_ip() as the built-in spam-bait (checks spamblock_exempt.cfg
	   and won't duplicate an active entry). */
	if(block) {
		var reason = "SPAM TRAP (" + recipient_address + ") taken";
		/* NOTE: duration must precede the filename - js_filter_ip() stops
		   parsing arguments once the filename (6th string) is set. */
		if(system.filter_ip(client.protocol, reason, client.host_name
			,client.ip_address, reverse_path, block_duration
			,system.ctrl_dir + "spamblock.cfg"))
			log(LOG_NOTICE, "BLOCKED IP ADDRESS: " + client.ip_address
				+ " in spamblock.cfg (" + reason + ")");
	}
	// The message is consumed (not delivered) via passthru=false in mailproc.ini.
}

main();
