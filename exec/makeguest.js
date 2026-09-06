// makeguest.js

// Script to create the Guest/Anonymous user account
// This is normally executed from logon.js (rev 1.7+)

// Don't create guest account if sysop account hasn't been created yet
if(!system.stats.total_users)	{
	printf("No users in database.\r\n");
	exit();
}

// If guest account exists, exit
if(system.matchuser("Guest")) {
	printf("Guest account already exists.\r\n");
	exit();
}

load("sbbsdefs.js");	// needed for UFLAG_* definitions

// Create the account
guest=system.new_user("Guest");
guest.handle="Guest";
guest.gender='?';
guest.comment="This is the auto-generated Guest/Anonymous user account.";

// Setup intelligent security parameters
guest.security.restrictions|=UREST_EDIT_DEFAULTS;	// can't edit defaults (main 'Guest' indicator)
guest.security.restrictions|=UREST_READ_SENT_MAIL;	// can't read sent mail
guest.security.restrictions|=UREST_POST;	// can't post messages
guest.security.restrictions|=UREST_SEND_NETMAIL;	// can't send network mail
guest.security.restrictions|=UREST_AUTO_MESSAGE;	// can't write to the auto-message
guest.security.restrictions|=UREST_REMOVE_FILES;	// can't remove files
guest.security.restrictions|=UREST_CHAT;	// can't chat
guest.security.restrictions|=UREST_VOTE;	// can't vote
guest.security.exemptions|=UEXEMPT_MULTINODE;		// multiple simultaneous logins
guest.security.exemptions|=UEXEMPT_LOGONS;		// unlimited logons per day
guest.security.exemptions|=UEXEMPT_TIME_ONLINE;		// unlimited time online
guest.security.exemptions|=UEXEMPT_PERMANENT;		// permanent (never expires)

printf("Guest account (user #%d) created successfully.\r\n",guest.number);
