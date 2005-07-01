/* $Id$ */

/*            Global Definitions              */
/*            Place globals here              */
/* Remember, the more global definitions here */
/* the longer all pages will take to display! */

if(user.number==0)
    template.user_greeting="Welcome, Guest.";
else
    if(!(user.security.restrictions&UFLAG_G))
        template.user_greeting="Welcome, "+user.alias+ ".<br /> You last visited on " +strftime("%A, %B %d, %Y",user.stats.laston_date);
    else
        template.user_greeting="Welcome, "+user.alias+ ".";

template.user_alias=user.alias;

/* Gives RAW port number - must be prepended with ":" for URI's */

template.http_port = http_port;
template.irc_port = irc_port;
template.ftp_port = ftp_port;
template.nntp_port = nntp_port;
template.gopher_port = gopher_port;
template.finger_port = finger_port;
template.telnet_port = telnet_port;
template.rlogin_port = rlogin_port;
template.smtp_port = smtp_port;
template.pop3_port = pop3_port;

