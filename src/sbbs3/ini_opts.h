/* ini_opts.h */

/* Synchronet initialization file (.ini) bit-field descriptions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbsdefs.h"

static ini_bitdesc_t bbs_options[] = {

	{ BBS_OPT_KEEP_ALIVE			,"KEEP_ALIVE"			},
	{ BBS_OPT_XTRN_MINIMIZED		,"XTRN_MINIMIZED"		},
	{ BBS_OPT_AUTO_LOGON			,"AUTO_LOGON"			},
	{ BBS_OPT_DEBUG_TELNET			,"DEBUG_TELNET"			},
	{ BBS_OPT_SYSOP_AVAILABLE		,"SYSOP_AVAILABLE"		},
	{ BBS_OPT_ALLOW_RLOGIN			,"ALLOW_RLOGIN"			},
	{ BBS_OPT_USE_2ND_RLOGIN		,"USE_2ND_RLOGIN"		},
	{ BBS_OPT_NO_QWK_EVENTS			,"NO_QWK_EVENTS"		},
	{ BBS_OPT_NO_TELNET_GA			,"NO_TELNET_GA"			},
	{ BBS_OPT_NO_EVENTS				,"NO_EVENTS"			},
	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_GET_IDENT				,"GET_IDENT"			},
	{ BBS_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ BBS_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ BBS_OPT_MUTE					,"MUTE"					},
	/* terminator */										
	{ -1							,NULL					}
};

static ini_bitdesc_t ftp_options[] = {

	{ FTP_OPT_DEBUG_RX				,"DEBUG_RX"				},
	{ FTP_OPT_DEBUG_DATA			,"DEBUG_DATA"			},	
	{ FTP_OPT_INDEX_FILE			,"INDEX_FILE"			},
	{ FTP_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ FTP_OPT_ALLOW_QWK				,"ALLOW_QWK"			},
	{ FTP_OPT_NO_LOCAL_FSYS			,"NO_LOCAL_FSYS"		},
	{ FTP_OPT_DIR_FILES				,"DIR_FILES"			},
	{ FTP_OPT_KEEP_TEMP_FILES		,"KEEP_TEMP_FILES"		},
	{ FTP_OPT_HTML_INDEX_FILE		,"HTML_INDEX_FILE"		},
	{ FTP_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ FTP_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ FTP_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ FTP_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ FTP_OPT_MUTE					,"MUTE"					},
	/* terminator */										
	{ -1							,NULL					}
};

static ini_bitdesc_t web_options[] = {

	{ WEB_OPT_DEBUG_RX				,"DEBUG_RX"				},
	{ WEB_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ WEB_OPT_VIRTUAL_HOSTS			,"VIRTUAL_HOSTS"		},
	{ WEB_OPT_NO_CGI				,"NO_CGI"				},

	/* shared bits */
	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_GET_IDENT				,"GET_IDENT"			},
	{ BBS_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ BBS_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ BBS_OPT_MUTE					,"MUTE"					},

	/* terminator */										
	{ -1							,NULL					}
};

static ini_bitdesc_t mail_options[] = {

	{ MAIL_OPT_DEBUG_RX_HEADER		,"DEBUG_RX_HEADER"		},
	{ MAIL_OPT_DEBUG_RX_BODY		,"DEBUG_RX_BODY"		},	
	{ MAIL_OPT_ALLOW_POP3			,"ALLOW_POP3"			},
	{ MAIL_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ MAIL_OPT_DEBUG_RX_RSP			,"DEBUG_RX_RSP"			},
	{ MAIL_OPT_RELAY_TX				,"RELAY_TX"				},
	{ MAIL_OPT_DEBUG_POP3			,"DEBUG_POP3"			},
	{ MAIL_OPT_ALLOW_RX_BY_NUMBER	,"ALLOW_RX_BY_NUMBER"	},
	{ MAIL_OPT_NO_NOTIFY			,"NO_NOTIFY"			},
	{ MAIL_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ MAIL_OPT_USE_TCP_DNS			,"USE_TCP_DNS"			},
	{ MAIL_OPT_NO_SENDMAIL			,"NO_SENDMAIL"			},
	{ MAIL_OPT_ALLOW_RELAY			,"ALLOW_RELAY"			},
	{ MAIL_OPT_SMTP_AUTH_VIA_IP		,"SMTP_AUTH_VIA_IP"		},
	{ MAIL_OPT_DNSBL_REFUSE			,"DNSBL_REFUSE"			},
	{ MAIL_OPT_DNSBL_IGNORE			,"DNSBL_IGNORE"			},
	{ MAIL_OPT_DNSBL_BADUSER		,"DNSBL_BADUSER"		},
	{ MAIL_OPT_DNSBL_CHKRECVHDRS	,"DNSBL_CHKRECVHDRS"	},
	{ MAIL_OPT_DNSBL_THROTTLE		,"DNSBL_THROTTLE"		},
	{ MAIL_OPT_DNSBL_DEBUG			,"DNSBL_DEBUG"			},
	{ MAIL_OPT_SEND_INTRANSIT		,"SEND_INTRANSIT"		},
	{ MAIL_OPT_RELAY_AUTH_PLAIN		,"RELAY_AUTH_PLAIN"		},
	{ MAIL_OPT_RELAY_AUTH_LOGIN		,"RELAY_AUTH_LOGIN"		},
	{ MAIL_OPT_RELAY_AUTH_CRAM_MD5	,"RELAY_AUTH_CRAM_MD5"	},
	{ MAIL_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ MAIL_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ MAIL_OPT_MUTE					,"MUTE"					},
	/* terminator */
	{ -1							,NULL					}
};

static ini_bitdesc_t service_options[] = {

	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_GET_IDENT				,"GET_IDENT"			},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_MUTE					,"MUTE"					},
	/* terminator */				
	{ -1							,NULL					}
};

static ini_bitdesc_t log_mask_bits[] = {
	{ (1<<LOG_EMERG)				,"EMERG"				},
	{ (1<<LOG_ALERT)				,"ALERT"				},
	{ (1<<LOG_CRIT)					,"CRIT"					},
	{ (1<<LOG_ERR)					,"ERR"					},
	{ (1<<LOG_WARNING)				,"WARNING"				},
	{ (1<<LOG_NOTICE)				,"NOTICE"				},
	{ (1<<LOG_INFO)					,"INFO"					},
	{ (1<<LOG_DEBUG)				,"DEBUG"				},
	/* the Gubinator */				
	{ -1							,NULL					}
};
