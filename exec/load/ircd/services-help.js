/* Public command reference shared by the authority and per-BBS services.
 * GPL-2.0-or-later. Help contains no account or channel records. */
"use strict";

var IRC_SERVICES_HELP = {
	NickServ: {
		commands:[
			"IDENTIFY - authenticate a BBS account", "LOGOUT - remove one or all BBS logins",
			"REGISTER (GROUP) - register your current nickname", "DROP - release a nickname registration",
			"INFO - show public nickname ownership and last use", "GHOST - disconnect another session using your nickname",
			"ACCOUNTS - inspect your logins and linked identities", "LINK - preview and confirm an identity merge",
			"UNLINK - detach a BBS account", "SET MEMOACCOUNT - choose a nickname's telegram destination"
		],
		aliases:{GROUP:"REGISTER", "LINK CONFIRM":"LINK", SET:"SET MEMOACCOUNT", MEMOACCOUNT:"SET MEMOACCOUNT"},
		pages:{
			IDENTIFY:{
				syntax:["/msg {NS} IDENTIFY <escaped-account> <password>"],
				text:[
					"Authenticate with the selected BBS's user account and password. There is no separate NickServ password.",
					"Spaces in account aliases may be written as dots: Some User becomes Some.User. Other delimiters use %HH escapes; a literal + becomes %2B. Do not append +QWKID to the IDENTIFY account argument.",
					"Use NickServ@<server> to select a BBS. Unqualified /ns IDENTIFY selects your connection server, which must have local NickServ enabled.",
					"One verified account per BBS is retained in this IRC session. Identifying elsewhere keeps other BBS logins; identifying again at the same BBS replaces that login.",
					"The most recent login becomes active and changes your username to escaped-alias+QWKID, removing the anonymous ~ prefix. Deleted, inactive, guest/shared, and passwordless accounts cannot identify.",
					"Matching IRC USER/PASS authentication also counts as identification. Neither method automatically registers a nickname: use /ns REGISTER afterward.",
					"Authentication to a linked account gives access to its network identity's nicknames, channel rights, and all linked BBS telegram mailboxes. Use /ns HELP LINK to understand that shared authority.",
					"Use a protected connection for passwords. If required by services configuration, your client socket and every server link to the handling BBS must use TLS."
				],
				examples:["/msg {NS} IDENTIFY Some.User <your-BBS-password>","/ns REGISTER"]
			},
			LOGOUT:{
				syntax:["/ns LOGOUT [ALL]","/msg {NS} LOGOUT"],
				text:[
					"Remove the selected BBS login from this IRC connection. Unqualified /ns LOGOUT selects the connection BBS; NickServ@<server> selects another BBS.",
					"LOGOUT ALL removes every retained BBS login. Logging out of the active BBS selects the most recently identified remaining account, or restores an anonymous ~ username if none remain.",
					"Registrations, linked identities, BBS accounts, and stored telegrams are retained. Authorization is revoked for logins no longer held; existing channel op/voice status is not automatically stripped.",
					"If you no longer own the registered nickname you are using, identify again or change nicknames before its 60-second enforcement grace expires.",
					"LOGOUT is available without TLS so that an insecure connection can still revoke its logins."
				], examples:["/msg {NS} LOGOUT","/ns LOGOUT ALL"]
			},
			REGISTER:{
				syntax:["/ns REGISTER","/ns GROUP"], secure:true,
				text:[
					"Requires a verified BBS login, obtained through NickServ IDENTIFY or matching USER/PASS authentication.",
					"Register your current nickname to the active BBS login's network identity. GROUP is an alias for REGISTER; it does not merge BBS identities. Neither command takes a password or another nickname.",
					"Change to the desired nickname before registering. An existing registration cannot be overwritten. The authority's MaxNicks limit applies per identity, including every linked BBS account (default 10).",
					"A new nickname initially sends telegrams to the active BBS account. SET <nickname> MEMOACCOUNT can change that destination to another account in the same identity.",
					"Ownership follows stable BBS account identities, not a mutable chat handle. Linking accounts shares all registrations; changing IRC nicknames does not change the owner's identity.",
					"Other sessions using a registered nickname get 60 seconds to identify or change nicknames. Updated servers can rename them to Guest names; legacy servers may require a KILL instead.",
					"NickExpireDays may expire an unused registration. It is renewed while an authenticated owner actually uses that nickname; using another nickname does not renew it.",
					"Once your current nickname is registered and owned by an authenticated identity, channel ops allow /cs REGISTER."
				], examples:["/nick MyNick","/ns REGISTER","/cs REGISTER #mychannel Discussion channel"]
			},
			DROP:{
				syntax:["/ns DROP [nickname]"], secure:true,
				text:[
					"Requires identification to any BBS account in the nickname's owning identity. Without an argument, drop the registration for your current nickname.",
					"Delete that nickname's ownership, last-used record, and configured telegram destination. The nickname becomes available for another identity to register.",
					"This does not disconnect its current user, delete BBS accounts or telegrams, unlink accounts, or remove the identity's channel ownership and access rights.",
					"There is no confirmation step. Use INFO to check a nickname and ACCOUNTS to review your identity before dropping it."
				], examples:["/ns INFO MyNick","/ns DROP OldNick"]
			},
			INFO:{
				syntax:["/ns INFO [nickname]"],
				text:[
					"Show the registered network identity and last-used time for a nickname. Omit the argument to inspect your current nickname.",
					"This is public and does not require identification or TLS. It does not disclose the private list of linked BBS accounts; owners use ACCOUNTS for that.",
					"An INFO query does not renew the registration. If nickname expiry is enabled, use of that exact nickname by an authenticated owner renews it."
				], examples:["/ns INFO MyNick"]
			},
			GHOST:{
				syntax:["/ns GHOST <nickname>"], secure:true,
				text:[
					"Disconnect another IRC session using a nickname registered to one of your authenticated network identities. This is useful after a dropped connection or a stuck bouncer session.",
					"Identify to an owning BBS account first, even if you are currently using a different nickname. A matching visible nick, username, or alias on an unrelated BBS is not proof of ownership.",
					"The named session must be online and must not be your own session. GHOST sends a network KILL; it does not merely rename the other user or unregister the nickname.",
					"After the other session disconnects, use /nick to take the recovered nickname. Its registration remains intact."
				], examples:["/msg {NS} IDENTIFY Some.User <your-BBS-password>","/ns GHOST MyNick","/nick MyNick"]
			},
			ACCOUNTS:{
				syntax:["/ns ACCOUNTS"], secure:true,
				text:[
					"Requires a verified BBS login. List the BBS logins retained by this IRC session and identify which one is active.",
					"For every identity you authenticated, show its linked BBS accounts, registered nicknames, each nickname's telegram destination, and channel ownership/access rights.",
					"These details are private to authenticated members. Multiple retained logins may still belong to independent identities; IDENTIFY alone does not merge them.",
					"Use LINK to merge identities only after reviewing all members and rights. Use SET MEMOACCOUNT to change telegram delivery, or UNLINK to detach an account."
				], examples:["/ns ACCOUNTS","/ns HELP LINK"]
			},
			LINK:{
				syntax:["/ns LINK <escaped-account+QWKID> <escaped-account+QWKID>","/ns LINK CONFIRM <code>"], secure:true,
				text:[
					"Requires fresh verification of both exact BBS accounts on this IRC connection, with updated services on the connection server and both BBSes. Use server-qualified IDENTIFY for each account first.",
					"The first command previews both entire network identities: all linked accounts, nickname registrations, telegram destinations, and channel rights. It does not change ownership yet.",
					"CONFIRM merges the whole identities, not just the two named accounts. EVERY linked member gains equal control of ALL combined nicknames and channel rights and may read ALL linked BBS telegram mailboxes. There is no privileged primary account.",
					"Authenticate accounts you control. Knowing somebody's alias or registered nickname is insufficient; do not merge with an account whose owner should not receive this authority.",
					"The confirmation code is single-use, expires after five minutes, and is bound to the session and previewed state. Changed logins, ownership, destinations, or access require another preview.",
					"The authority rechecks ownership and the MaxNicks limit before an atomic SQLite merge. Existing nickname destinations and last-used times are retained; duplicate channel access takes the higher level, with founder rights taking precedence.",
					"Use ACCOUNTS afterward to review the result. UNLINK can detach an account but leaves registrations with the remaining identity."
				], examples:["/msg NickServ@vert.synchro.net IDENTIFY Deuce <password>","/msg NickServ@eotl.synchro.net IDENTIFY RealDeuce <password>","/ns LINK Deuce+VERT RealDeuce+EOTL","/ns LINK CONFIRM <code-from-preview>"]
			},
			UNLINK:{
				syntax:["/ns UNLINK <escaped-account+QWKID>"], secure:true,
				text:[
					"Requires identification to the identity containing the named account. Any authenticated member can detach another member; the last remaining account cannot unlink.",
					"Move the account into a new, empty network identity. Nickname registrations and channel rights remain with the other members; they do not follow the detached account.",
					"If the account is a configured MEMOACCOUNT destination, UNLINK lists affected nicknames and refuses. Explicitly choose new destinations for all of them before retrying.",
					"This immediately changes shared services authorization and mailbox access. A separately retained login to the detached account can still access that account's own mailbox.",
					"Existing channel op/voice status is not automatically removed. Native BBS accounts and their stored telegrams are not deleted. Use ACCOUNTS to review the remaining identities."
				], examples:["/ns SET MyNick MEMOACCOUNT Some.User+HOME","/ns UNLINK RealDeuce+EOTL"]
			},
			"SET MEMOACCOUNT":{
				syntax:["/ns SET <nickname> MEMOACCOUNT <escaped-account+QWKID>"], secure:true,
				text:[
					"Requires identification to the nickname's owning identity. The destination account must already belong to that same identity; an unrelated account cannot be selected.",
					"Choose where MemoServ SEND <nickname> stores new telegrams by default. Different nicknames in one identity may choose different destinations, and LINK preserves those choices.",
					"READ, STATUS, and automatic checks visit this destination first, then other linked mailboxes. This setting does not restrict reads to one account or move existing telegrams between BBSes.",
					"An explicitly qualified MemoServ@<server> SEND <nickname> selects that identity's account on the named BBS instead. Missing or ambiguous matches are rejected.",
					"Use ACCOUNTS to inspect current destinations. Retarget every affected nickname before UNLINK removes a destination account."
				], examples:["/ns SET MyNick MEMOACCOUNT Some.User+HOME","/ns ACCOUNTS"]
			}
		}
	},
	ChanServ: {
		commands:[
			"REGISTER - register a channel to your current nickname's identity", "INFO - inspect a channel registration",
			"DROP - remove a channel registration", "ACCESS - manage persistent identity permissions",
			"EXCEPT - manage persistent channel exceptions", "SET MLOCK - lock supported channel modes",
			"SET KEEPTOPIC - preserve and restore the channel topic", "TOPIC - change or clear a topic",
			"OP / DEOP - set or remove current channel ops", "VOICE / DEVOICE - set or remove current voice",
			"INVITE - invite an online user", "UNBAN - remove bans matching an online user"
		],
		aliases:{"ACCESS ADD":"ACCESS","ACCESS DEL":"ACCESS","ACCESS LIST":"ACCESS",
			"EXCEPT ADD":"EXCEPT","EXCEPT DEL":"EXCEPT","EXCEPT LIST":"EXCEPT",MLOCK:"SET MLOCK",KEEPTOPIC:"SET KEEPTOPIC"},
		pages:{
			REGISTER:{
				syntax:["/cs REGISTER #channel <description>"], secure:true,
				text:[
					"Requires membership and channel ops, plus a current nickname registered to one of your authenticated network identities. NickServ IDENTIFY or matching USER/PASS authentication counts.",
					"Authenticate, use /ns REGISTER for your current nickname if necessary, then register the channel. An unregistered nickname or another identity's nickname cannot found a new registration.",
					"The founder is the identity owning your current nickname, even if a different retained BBS login is active. Every linked BBS account shares founder rights; the rights survive IRC nickname and BBS alias changes.",
					"The channel must exist and be unregistered. Supply a description (stored up to 200 characters). Registration does not automatically enable a mode lock or KEEPTOPIC.",
					"ChanServ stays outside the channel. When its last member leaves, the live channel disappears; on recreation, saved settings and managed exceptions return through the services action queue.",
					"On recreation, automatic creator ops are removed unless the creator is authenticated as the founder or has OP/SOP access. VOICE access receives voice instead. Later explicit operator grants are preserved.",
					"ChannelExpireDays can expire unused registrations. Authenticated founder or access-list member presence, including VOICE access, counts as use; unrelated visitors and INFO queries do not.",
					"Use ACCESS to grant identity rights and SET to configure persistence. Existing runtime modes, bans, and exceptions are not automatically imported as saved policy."
				], examples:["/ns REGISTER","/join #mychannel","/cs REGISTER #mychannel Discussion channel","/cs HELP SET"]
			},
			INFO:{
				syntax:["/cs INFO #channel"],
				text:[
					"Show a registered channel's description, founder identity, and last-used timestamp. Reading INFO does not renew the registration.",
					"No identification or TLS is needed for public information about an existing visible channel. Secret/private channels and absent channels are hidden from outsiders without authenticated access.",
					"Founder and access rights belong to network identities, not the visible nickname used at registration. Use ACCESS LIST for the permission list when you have channel access."
				], examples:["/cs INFO #mychannel"]
			},
			DROP:{
				syntax:["/cs DROP #channel"], secure:true,
				text:[
					"Only an authenticated member of the saved founder identity may drop a registration. Current channel ops alone are insufficient; the founder need not be in the channel.",
					"Delete the registration, its access list, mode lock, kept-topic policy, managed exception grants, and usage record. There is no confirmation step.",
					"This stops saved ChanServ policy; it does not kick users, delete an occupied channel, or remove its existing runtime modes, bans, or exceptions. Another eligible founder may subsequently register it.",
					"BBS accounts, linked identities, nickname registrations, and telegrams are retained. Review INFO and ACCESS LIST before dropping a channel."
				], examples:["/cs INFO #mychannel","/cs DROP #mychannel"]
			},
			ACCESS:{
				syntax:["/cs ACCESS #channel ADD <registered-nick|escaped-account+QWKID> VOICE|OP|SOP",
					"/cs ACCESS #channel DEL <registered-nick|escaped-account+QWKID>","/cs ACCESS #channel LIST"], secure:true,
				text:[
					"Requires identification to an identity with channel access. LIST is available at any access level; editing requires SOP or founder rights. You need not currently hold channel ops or be in the channel.",
					"A registered nickname selects its owning network identity. A qualified BBS account must previously have identified to central services. Grants apply to the entire linked identity, not just that nickname or BBS login.",
					"VOICE (10) grants automatic voice. OP (50), also accepted as AOP, grants automatic ops and live channel actions. SOP (80) additionally edits lower access entries and managed exceptions. The founder has level 100.",
					"You cannot edit the founder entry, an entry at or above your own level, or grant a level at or above your own. Thus SOPs can manage VOICE and OP; only founders can grant or remove SOP.",
					"ADD sets the selected identity's level; DEL removes its saved grant. Changing access does not automatically strip existing runtime op/voice status, and DEOP/DEVOICE alone does not revoke a saved grant.",
					"Only the founder changes SET settings or drops registration. Any authenticated founder/access member present renews channel use for expiry, including VOICE-level members."
				], examples:["/cs ACCESS #mychannel ADD MyFriend OP","/cs ACCESS #mychannel ADD Some.User+HOME VOICE","/cs ACCESS #mychannel DEL MyFriend","/cs ACCESS #mychannel LIST"]
			},
			EXCEPT:{
				syntax:["/cs EXCEPT #channel ADD <entry>","/cs EXCEPT #channel DEL <entry>","/cs EXCEPT #channel LIST"], secure:true,
				text:[
					"Requires authenticated SOP/founder access to ADD or DEL; OP or higher may LIST. Permissions belong to identities, but the saved grants themselves match current nick!username@host masks.",
					"A plain hostmask exempts matching users from +b bans on joining and speaking. It does not bypass invite-only, keys, limits, +n, or +m.",
					"~msgbypass:external:<hostmask> bypasses +n (no outside messages). ~msgbypass:moderated:<hostmask> bypasses +m. The short ~m:<type>:<hostmask> form is accepted and stored in the long form.",
					"Both bypasses affect PRIVMSG and NOTICE. An outside sender to +mn needs both; a ban still applies unless a separate plain exception matches. No membership, visibility, subscription, or incoming message route is granted.",
					"Only external and moderated bypasses are supported. Censor, color, notice, account, and nested selectors are rejected and never stored. Preserve complete usernames, including +QWKID and %HH escapes, when writing masks.",
					"ADD saves a grant and applies it to a live channel. A matching live +e entry is adopted without redundant MODEs. Direct MODE entries remain independent unless explicitly added here.",
					"DEL removes both saved and matching live entries. MODE -e alone cannot revoke managed policy: ChanServ restores it. Saved grants also return after channel recreation and authority restart.",
					"There may be at most 25 managed grants, independently of bans. ADD refuses a new entry if the live list is full but may adopt an existing one. Later blocked restoration remains pending and logs once; manual entries are never evicted.",
					"A nickname-only mask can be used by anyone holding that nickname. Use a suitable full mask when identity matters. Legacy servers omit +e state and can block traffic that updated servers permit; updated paths are needed for complete behavior."
				], examples:["/cs EXCEPT #mychannel ADD ~m:external:Bot!*@trusted.example","/cs EXCEPT #mychannel ADD ~m:moderated:Bot!*@trusted.example","/cs EXCEPT #mychannel LIST","/cs EXCEPT #mychannel DEL ~m:external:Bot!*@trusted.example"]
			},
			SET:{
				syntax:["/cs SET #channel MLOCK <modes> [key/limit]","/cs SET #channel MLOCK NONE","/cs SET #channel KEEPTOPIC ON|OFF"], secure:true,
				text:[
					"Only authenticated members of the founder identity may change channel settings. They need not be in the channel or currently hold ops.",
					"MLOCK enforces the saved supported channel modes. KEEPTOPIC records and restores the topic. Both survive authority restart and channel recreation.",
					"Each MLOCK replaces the previous lock. + flags must be present, - flags absent, and omitted flags are unconstrained. Supported flags are i invite-only, m moderated, n no outside messages, p private, s secret, t ops-only topics, k key, and l member limit. +p and +s are mutually exclusive.",
					"For +k, supply a key of 1-64 characters; for +l, supply an integer from 1 to 99999. Key/limit arguments follow the order k/l occur in the mode string. -k and -l take no argument and require their values to be removed.",
					"MLOCK NONE stops enforcement without undoing current runtime modes. MLOCK does not save bans, exceptions, or user status: use EXCEPT and ACCESS for their supported persistent policies.",
					"KEEPTOPIC ON captures the current topic and tracks later changes for restoration. OFF stops restoration without clearing the live topic. If the channel is absent, ON retains its previously saved topic.",
					"Settings can be changed while the registered channel is absent, without creating it. Restored MODE/TOPIC updates may follow the initial JOIN replies. Use /cs HELP SET MLOCK or /cs HELP SET KEEPTOPIC for additional details."
				], examples:["/cs SET #mychannel MLOCK +ntkl secret 25","/cs SET #mychannel MLOCK NONE","/cs SET #mychannel KEEPTOPIC ON"]
			},
			"SET MLOCK":{
				syntax:["/cs SET #channel MLOCK <+/-imnpstkl> [key/limit]","/cs SET #channel MLOCK NONE"], secure:true,
				text:[
					"Founder identification is required. Replace the saved mode lock: + flags must be present and - flags must be absent. Modes omitted from the new lock are unconstrained.",
					"Supported flags: i invite-only; m moderated; n no outside messages; p private; s secret; t topic changes require ops; k channel key; l member limit. Private and secret cannot both be enabled.",
					"Supply a key after +k (1-64 characters) and a whole-number limit after +l (1-99999), in the order k/l occur in the mode string. -k and -l take no argument and require the key/limit to be removed.",
					"The lock is applied to a live channel and reasserted after changes. An absent channel is not created; the lock returns after its next creation, through the normal services queue.",
					"MLOCK NONE stops enforcing the lock but does not immediately undo current runtime modes. Ban/exception lists and op/voice grants are not boolean mode locks: use EXCEPT, ACCESS, or ordinary MODE as appropriate."
				], examples:["/cs SET #mychannel MLOCK +nt","/cs SET #mychannel MLOCK +ntkl secret 25","/cs SET #mychannel MLOCK +nt-kl","/cs SET #mychannel MLOCK NONE"]
			},
			"SET KEEPTOPIC":{
				syntax:["/cs SET #channel KEEPTOPIC ON|OFF"], secure:true,
				text:[
					"Founder identification is required. ON saves the current topic when the channel exists, tracks later topic changes, and restores the saved topic after channel recreation or authority restart.",
					"OFF disables restoration. Turning it off does not clear the live topic. The setting may be changed while the channel is absent; ON then retains the previously saved topic.",
					"ChanServ remains outside the channel. Restoration may arrive as a TOPIC update after the initial JOIN replies. Use TOPIC for an immediate topic change; it requires OP access or higher."
				], examples:["/cs SET #mychannel KEEPTOPIC ON","/cs TOPIC #mychannel Welcome to the channel","/cs SET #mychannel KEEPTOPIC OFF"]
			},
			TOPIC:{
				syntax:["/cs TOPIC #channel [text]"], secure:true,
				text:[
					"Requires authenticated OP access or higher and an existing registered channel. You need not be in the channel or currently hold channel ops.",
					"Set the channel topic to the remaining text (up to 307 characters). Omit text to clear it. ChanServ requests the change through its trusted services server.",
					"The topic is recorded in the registration. Restoration after channel recreation requires SET KEEPTOPIC ON; TOPIC by itself does not enable that setting."
				], examples:["/cs TOPIC #mychannel Welcome to the channel","/cs TOPIC #mychannel"]
			},
			OP:{syntax:["/cs OP #channel [nickname]"],secure:true,text:[
				"Requires authenticated OP access or higher on an existing registered channel. Omit nickname to target yourself; the target must be online and in the channel.",
				"Grant current +o status. The requester need not be in the channel or currently hold ops. This changes live status only; use ACCESS ADD <identity> OP for a persistent automatic grant."
			],examples:["/cs OP #mychannel","/cs OP #mychannel MyFriend"]},
			DEOP:{syntax:["/cs DEOP #channel [nickname]"],secure:true,text:[
				"Requires authenticated OP access or higher on an existing registered channel. Omit nickname to target yourself; the target must be online and in the channel.",
				"Remove current +o status without changing the target's saved identity access. A founder or OP/SOP access grant can restore ops on subsequent enforcement.",
				"Use ACCESS to revoke saved access where your level permits it. Removing access does not itself strip current modes; access and live status are separate."
			],examples:["/cs DEOP #mychannel MyFriend"]},
			VOICE:{syntax:["/cs VOICE #channel [nickname]"],secure:true,text:[
				"Requires authenticated OP access or higher on an existing registered channel. Omit nickname to target yourself; the target must be online and in the channel.",
				"Grant current +v status, allowing speech under +m moderation. This changes live status only; use ACCESS ADD <identity> VOICE for a persistent automatic grant. It does not grant ops or services management rights."
			],examples:["/cs VOICE #mychannel MyFriend"]},
			DEVOICE:{syntax:["/cs DEVOICE #channel [nickname]"],secure:true,text:[
				"Requires authenticated OP access or higher on an existing registered channel. Omit nickname to target yourself; the target must be online and in the channel.",
				"Remove current +v status. Stored access remains intact and may restore voice later. A user with ops, another applicable exemption, or an unmoderated channel may still speak."
			],examples:["/cs DEVOICE #mychannel MyFriend"]},
			INVITE:{syntax:["/cs INVITE #channel [nickname]"],secure:true,text:[
				"Requires authenticated OP access or higher on an existing registered channel. Omit nickname to invite yourself; an explicit target must be online. The requester need not be in the channel.",
				"Send an IRC invitation through ChanServ. It permits the target to pass +i invite-only admission and the normal invitation ban check; it does not supply a key, bypass the member limit, or grant saved channel access.",
				"ChanServ stays outside the channel. Relays and the destination IRCd must support invites from trusted services without channel ops; older INVITE parsers can reject these requests."
			],examples:["/cs INVITE #mychannel","/cs INVITE #mychannel MyFriend","/join #mychannel <key-if-required>"]},
			UNBAN:{syntax:["/cs UNBAN #channel [nickname]"],secure:true,text:[
				"Requires authenticated OP access or higher on an existing registered channel. Omit nickname to target yourself; the target must be online but need not be in the channel.",
				"Remove every current +b mask matching the target's nick!username@host. This edits the ban list, rather than adding an exception or forcing the target to JOIN.",
				"Invite-only, channel keys, member limits, and other restrictions still apply. Use INVITE for an invitation or EXCEPT for a persistent ban exemption."
			],examples:["/cs UNBAN #mychannel","/cs UNBAN #mychannel MyFriend"]}
		}
	},
	MemoServ: {
		commands:["SEND - deliver a native BBS telegram","STATUS - check mailboxes without consuming telegrams","READ - receive and consume waiting telegrams"],
		aliases:{},
		pages:{
			SEND:{syntax:["/ms SEND <registered-nick|escaped-account+QWKID> <text>","/msg {MS} SEND <registered-nick|escaped-account+QWKID> <text>"],secure:true,text:[
				"Requires your own verified BBS login. MemoServ stores a native BBS telegram, not a separate IRC memo. The destination BBS must be online with MemoServ enabled, and its user must be eligible to receive telegrams.",
				"Sending to a registered nickname uses that nickname's configured MEMOACCOUNT destination. It sends to one BBS, not every linked account. Use /ns HELP SET MEMOACCOUNT to choose the default.",
				"MemoServ@<server> SEND <nickname> explicitly selects the nickname's linked account on that BBS. It fails if no account matches or more than one account on that BBS makes the choice ambiguous.",
				"An explicit escaped-account+QWKID names that exact BBS account, even if it has never identified to IRC services. Spaces become dots and other delimiters use %HH escapes. A conflicting qualified service server is rejected.",
				"Delivery is acknowledged only after native telegram storage succeeds. Existing telegrams stay on their original BBS; changing MEMOACCOUNT does not move them.",
				"Updated services are needed for explicit per-server nickname delivery. Older authorities refuse unsupported overrides instead of silently choosing a different destination."
			],examples:["/ms SEND MyFriend Hello from IRC","/ms SEND Some.User+HOME Hello on your home BBS","/msg {MS} SEND MyFriend Deliver to this BBS explicitly"]},
			STATUS:{syntax:["/ms STATUS","/msg {MS} STATUS"],secure:true,text:[
				"Requires a verified BBS login. Check whether native BBS telegrams are waiting without consuming or deleting them. Automatic checks after identification are also nondestructive.",
				"Unqualified STATUS checks every BBS account in your current owned nickname's identity, starting with its MEMOACCOUNT destination. If your current nickname is not owned, it uses your active login's identity instead.",
				"Identification to any linked member authorizes all mailboxes in that identity. Independent, unlinked identities retained in the session do not become one default mailbox group.",
				"MemoServ@<server> STATUS limits checks to that BBS within your authenticated identities. Replies name each mailbox; a failure on one BBS is reported while checks continue for the others.",
				"Use READ only when ready to consume the telegrams. Older authorities/helpers may retain single-login, per-BBS behavior."
			],examples:["/ms STATUS","/msg {MS} STATUS","/ms HELP READ"]},
			READ:{syntax:["/ms READ","/msg {MS} READ"],secure:true,text:[
				"Requires a verified BBS login. READ receives AND CONSUMES native BBS telegrams using the BBS's get_telegram API. These telegrams are no longer waiting for a later IRC read or ordinary BBS delivery.",
				"Use STATUS for a nondestructive check. READ is not a numbered memo browser and does not keep a separate IRC archive or offer UNREAD/DELETE commands.",
				"Unqualified READ visits every BBS account in your current owned nickname's identity, beginning with its MEMOACCOUNT destination. Without an owned current nickname, it uses the active BBS login's identity.",
				"Identifying to any member of a linked identity permits all of its mailboxes to be read. MemoServ@<server> READ limits reads to that BBS within your authenticated identities.",
				"Replies identify each mailbox. If a BBS is unavailable, the error is reported and other mailboxes are still processed. Logout, account replacement, and unlinking revoke authorization for subsequent requests and replies.",
				"The BBS still checks account eligibility and record identity. Older services may support only the active-login/per-BBS read behavior."
			],examples:["/ms STATUS","/ms READ","/msg {MS} READ"]}
		}
	}
};

function IRC_services_help(service, args, context) {
	var reference=IRC_SERVICES_HELP[service], shortname={NickServ:"ns",ChanServ:"cs",MemoServ:"ms"}[service];
	if (!reference) return "Unknown service.";
	var topic=(args || []).join(" ").trim().toUpperCase(), lines=[], home=String(context && context.home || "bbs-server").toLowerCase();
	if (!/^[a-z0-9_.:-]+$/.test(home)) home="bbs-server";
	if (!topic || topic=="HELP") {
		lines=[service+" command help", "Syntax: /"+shortname+" HELP <command> [subcommand]",
			"Required arguments use <angle brackets>; optional arguments use [brackets]. Do not type the brackets. Command words are case-insensitive."];
		lines.push(service=="NickServ" ? "Local NickServ authenticates BBS accounts; the central authority manages registrations and linked network identities. Use server-qualified IDENTIFY to select a BBS."
			: service=="ChanServ" ? "Channel permissions belong to authenticated network identities. ChanServ manages saved policy without joining channels; live channel ops and saved access are separate."
			: "MemoServ handles native BBS telegrams. STATUS is nondestructive; READ consumes waiting telegrams. Linked identities share mailbox access, while SEND has one selected destination.");
		lines=lines.concat(reference.commands);
		lines.push("Detailed examples: /"+shortname+" HELP "+(service=="ChanServ" ? "REGISTER or /cs HELP SET MLOCK" : service=="NickServ" ? "IDENTIFY or /ns HELP LINK" : "READ"));
		if (service!="ChanServ") lines.push("Select a BBS with /msg "+service+"@"+home+" HELP <command>. Registration and channel policy belong to the central authority.");
		lines.push("HELP is public and available without identification or TLS.");
		lines.push("End of "+service+" HELP.");
	} else {
		var key=reference.aliases[topic] || topic, page=reference.pages[key];
		if (!page) return "Unknown "+service+" help topic. Use /"+shortname+" HELP for available commands.";
		lines=[service+" HELP "+topic];
		page.syntax.forEach(function(syntax) { lines.push("Syntax: "+syntax); });
		lines=lines.concat(page.text);
		if (page.secure) lines.push("If required by services configuration, this command needs TLS on your client connection and every server hop to the handling service (and destination BBS for private mailbox operations).");
		page.examples.forEach(function(example) { lines.push("Example: "+example); });
		lines.push("End of "+service+" HELP "+topic+". Use /"+shortname+" HELP to list commands.");
	}
	var wrapped=[];
	lines.forEach(function(line) {
		line=line.replace(/\{NS\}/g,"NickServ@"+home).replace(/\{MS\}/g,"MemoServ@"+home);
		/* Keep ordinary help lines below the NOTICE chunk limit and wrap at
		   words, so clients display complete paragraphs without cut words. */
		while (line.length>260) {
			var cut=line.lastIndexOf(" ",260); if (cut<1) cut=260;
			wrapped.push(line.slice(0,cut)); line=line.slice(cut).replace(/^ +/,"");
		}
		wrapped.push(line);
	});
	return wrapped.join("\n");
}
