// makeguest.js

// Don't create guest account if sysop account hasn't been created yet

// $Id$

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
user=system.new_user("Guest");
user.gender='?';
user.comment="This is the auto-generated Guest/Anonymous user account.";

// Setup intelligent security parameters
user.security.restrictions|=UFLAG_G;	// can't edit defaults (main 'Guest' indicator)
user.security.restrictions|=UFLAG_K;	// can't read sent mail
user.security.restrictions|=UFLAG_P;	// can't post
user.security.restrictions|=UFLAG_M;	// can't post on networked subs (redundant)
user.security.restrictions|=UFLAG_W;	// can't write to the auto-message
user.security.exemptions|=UFLAG_G;		// multiple simultaneous logins
user.security.exemptions|=UFLAG_L;		// unlimited logons per day
user.security.exemptions|=UFLAG_T;		// unlimited time online
user.security.exemptions|=UFLAG_P;		// permanent (never expires)

printf("Guest account (#%d) created successfully\r\n",user.number);
