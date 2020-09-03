/* ftelnethelper.js */

/* Helper functions to get values from sbbs.ini/services.ini for fTelnet */

/* $Id: ftelnethelper.js,v 1.11 2019/08/11 16:43:05 echicken Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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

// Values from sbbs.ini
var FLoadedSBBSIni = false;
var FBBSOptions    = "";
var FRLoginPort    = -1;
var FTelnetPort    = -1;
var FGlobalInterface = "";
var FRLoginInterface = "";
var FTelnetInterface = "";

// Values from services.ini
var FLoadedServicesIni = false;
var FWSEnabled         = false;
var FWSPort            = -1;
var FWSSEnabled        = false;
var FWSSPort           = -1;

// Credit to echicken for port lookup code
function GetSBBSIniValues() {
	if (!FLoadedSBBSIni) {
		FLoadedSBBSIni = true;
		
		try {
			var f = new File(file_cfgname(system.ctrl_dir, "sbbs.ini"));
			if (f.open("r", true)) {
				FBBSOptions = f.iniGetValue("BBS", "Options", "");
				FRLoginPort = f.iniGetValue("BBS", "RLoginPort", -1);
				FTelnetPort = f.iniGetValue("BBS", "TelnetPort", -1);
				FGlobalInterface = f.iniGetValue("Global", "Interface", "").split(',')[0];
				FRLoginInterface = f.iniGetValue("BBS", "RLoginInterface", "").split(',')[0];
				FTelnetInterface = f.iniGetValue("BBS", "TelnetInterface", "").split(',')[0];
				f.close();
			}
		} catch (err) {
			log(LOG_ERR, "GetSBBSIniValues() error: " + err.toString());
		}
	}
}

function GetServicesIniValues() {
	if (!FLoadedServicesIni) {
		FLoadedServicesIni = true;
		
		try {
			var f = new File(file_cfgname(system.ctrl_dir, "services.ini"));
			if (f.open("r", true)) {
				// Try to get the WS service port
                FWSPort = f.iniGetValue("WS", "Port", -1);
                FWSEnabled = (FWSPort > 0) && f.iniGetValue("WS", "Enabled", true);
				
				// Try to get the WSS service port
                FWSSPort = f.iniGetValue("WSS", "Port", -1);
                FWSSEnabled = (FWSSPort > 0) && f.iniGetValue("WSS", "Enabled", true);

				f.close();
			}
		} catch (err) {
			log(LOG_ERR, "GetServicesIniValues() error: " + err.toString());
		}
	}
}

function GetRLoginPort() {
	GetSBBSIniValues();
	return (IsRLoginEnabled() ? FRLoginPort : -1);
}

function GetTelnetPort() {
	GetSBBSIniValues();
	return FTelnetPort;
}

function GetWebSocketServicePort(wss) {
	GetServicesIniValues();
	return (wss ? FWSSPort : FWSPort);
}

function IsWebSocketServiceEnabled(wss) {
	GetServicesIniValues();
	return (wss ? FWSSEnabled : FWSEnabled);
}

function IsRLoginEnabled() {
	GetSBBSIniValues();
	return (FBBSOptions.indexOf("ALLOW_RLOGIN") !== -1);
}

function StringToBytes(InLine) {
	var Result = [];

	for (var i = 0; i < InLine.length; i++) {
		Result.push(InLine.charCodeAt(i));
	}

	return Result;
}

function UsingSecondRLoginName() {
	GetSBBSIniValues();
	return (FBBSOptions.indexOf("USE_2ND_RLOGIN") !== -1);
}

function GetInterface(i) {
	if (['', '0.0.0.0', '::'].indexOf(i) > -1) return 'localhost';
	return i;
}

function GetGlobalInterface() {
	GetSBBSIniValues();
	return GetInterface(FGlobalInterface);
}

function GetTelnetInterface() {
	GetSBBSIniValues();
	return GetInterface(FTelnetInterface);
}

function GetRLoginInterface() {
	GetSBBSIniValues();
	return GetInterface(FRLoginInterface);
}