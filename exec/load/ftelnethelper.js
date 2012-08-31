/* ftelnethelper.js */

/* Helper functions to get values from sbbs.ini/services.ini for fTelnet */

/* $Id$ */

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
var FSSHPort       = -1;
var FTelnetPort    = -1;

// Values from services.ini
var FLoadedServicesIni               = false;
var FFlashSocketPolicyServiceEnabled = false;
var FFlashSocketPolicyServicePort    = -1;
var FServicePorts                    = "";

// Credit to echicken for port lookup code
function GetSBBSIniValues() {
	if (!FLoadedSBBSIni) {
		FLoadedSBBSIni = true;
		
		try {
			var f = new File(file_cfgname(system.ctrl_dir, "sbbs.ini"));
			if (f.open("r", true)) {
				FBBSOptions = f.iniGetValue("BBS", "Options", "");
				FRLoginPort = f.iniGetValue("BBS", "RLoginPort", -1);
				FSSHPort    = f.iniGetValue("BBS", "SSHPort", -1);
				FTelnetPort = f.iniGetValue("BBS", "TelnetPort", -1);
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
				// Try to get the flash socket policy server port
                FFlashSocketPolicyServicePort = f.iniGetValue("FlashPolicy", "Port", -1);
                FFlashSocketPolicyServiceEnabled = f.iniGetValue("FlashPolicy", "Enabled", true);

				// Try to get the ports from all the enabled services (also look for entry for flash socket policy server if it wasn't found above)
				var Sections = f.iniGetSections();
				for (var i = 0; i < Sections.length; i++) {
					// If we don't know the flash socket policy port yet, check if this could be it
					if (FFlashSocketPolicyServicePort === -1) {
						if (f.iniGetValue(Sections[i], "Port", -1) === 843) {
							FFlashSocketPolicyServicePort = 843;
							FFlashSocketPolicyServiceEnabled = f.iniGetValue(Sections[i], "Enabled", true);
						}
					}
					
					// If this service is enabled (and not UDP), add it to the port list
					if ((f.iniGetValue(Sections[i], "Enabled", true)) && (f.iniGetValue(Sections[i], "Options", "").toUpperCase().indexOf("UDP") === -1)) {
						if (FServicePorts != "") FServicePorts += ",";
						FServicePorts += f.iniGetValue(Sections[i], "Port", -1);
					}
				}
				
				f.close();
			}
		} catch (err) {
			log(LOG_ERR, "GetServicesIniValues() error: " + err.toString());
		}
	}
}

function GetFlashSocketPolicyServicePort() {
	GetServicesIniValues();
	return FFlashSocketPolicyServicePort;
}

function GetRLoginPort() {
	GetSBBSIniValues();
	return FRLoginPort;
}

function GetServicePorts() {
	GetServicesIniValues();
	return FServicePorts;
}

function GetSSHPort() {
	GetSBBSIniValues();
	return FSSHPort;
}

function GetTelnetPort() {
	GetSBBSIniValues();
	return FTelnetPort;
}

function GetTerminalServerPorts() {
	var Ports = GetTelnetPort();
	if (IsRLoginEnabled()) Ports += "," + GetRLoginPort();
	if (IsSSHEnabled()) Ports += "," + GetSSHPort();
	Ports += "," + GetServicePorts();
	return Ports;
}

function IsFlashSocketPolicyServerEnabled() {
	GetServicesIniValues();
	return FFlashSocketPolicyServiceEnabled;
}

function IsRLoginEnabled() {
	GetSBBSIniValues();
	return (FBBSOptions.indexOf("ALLOW_RLOGIN") !== -1);
}

function IsSSHEnabled() {
	GetSBBSIniValues();
	return (FBBSOptions.indexOf("ALLOW_SSH") !== -1);
}

function UsingSecondRLoginName() {
	GetSBBSIniValues();
	return (FBBSOptions.indexOf("USE_2ND_RLOGIN") !== -1);
}
