// makeguest.js

// Script to create the Guest/Anonymous user account
// This is normally executed from logon.js (rev 1.7+)

// $Id$

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
guest.security.restrictions|=UFLAG_G;	// can't edit defaults (main 'Guest' indicator)
guest.security.restrictions|=UFLAG_K;	// can't read sent mail
guest.security.restrictions|=UFLAG_P;	// can't post
guest.security.restrictions|=UFLAG_M;	// can't post on networked subs (redundant)
guest.security.restrictions|=UFLAG_W;	// can't write to the auto-message
guest.security.exemptions|=UFLAG_G;		// multiple simultaneous logins
guest.security.exemptions|=UFLAG_L;		// unlimited logons per day
guest.security.exemptions|=UFLAG_T;		// unlimited time online
guest.security.exemptions|=UFLAG_P;		// permanent (never expires)

printf("Guest account (user #%d) created successfully.\r\n",guest.number);
