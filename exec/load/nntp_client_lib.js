// This is a JS include that provides functionality for interacting with a news
// (NNTP/newsgroup) server.
//
// Author: Eric Oulashin (AKA Nightfox)
// BBS: Digital Distortion
// BBS addresses: digitaldistortionbbs.com
//                digdist.synchro.net

// List of NNTP commands:
// http://www.tcpipguide.com/free/t_NNTPCommands-2.htm
//
// NNTP specification:
// https://datatracker.ietf.org/doc/html/rfc3977
//
// Response codes:
// http://www.tcpipguide.com/free/t_NNTPStatusResponsesandResponseCodes-3.htm

"use strict";

require("newsutil.js", "decode_news_body");


/////////////////////////////////////////////////////////////////////////////////////

// A class for containing options for the NNTP server
function NNTPConnectOptions()
{
	this.hostname = "";
	this.port = 119;
	this.username = "";
	this.password = "";
	this.port_set = false;
	this.tls = false;
	this.no_path = false;
}

// NNTPClient constructor
//
// Parameters:
//  pNNTPConnectOptions: A NNTPConnectOptions object containing options such as the hostname, port, username, password, etc.
//  pInterfaceIP: Numeric IP address of the local network interface to bind the socket to
//  pRecvBufSizeBytes: The number of bytes to use for the receive buffer
//  pRecvTimeoutSeconds: The amount of timeout (in seconds) to use when waiting for a response from the server
function NNTPClient(pNNTPConnectOptions, pInterfaceIP, pRecvBufSizeBytes, pRecvTimeoutSeconds)
{
	this.connectOptions = pNNTPConnectOptions;
	this.recvBufSizeBytes = pRecvBufSizeBytes;
	this.recvTimeoutSeconds = pRecvTimeoutSeconds;

	this.interfaceIP = pInterfaceIP; // IP address of the local network interface to bind the socket to
	this.socket = null;

	// The name of the current selected newsgroup
	this.selectedNewsgroupName = "";

	// Whether or not to convert all field (property) names for header objects
	this.lowercaseHeaderFieldNames = false;
	// Whether or not to add a "to" field with "All" if a "to" field is missing from a header
	this.addHeaderToIfMissing = false;

	// Possible header field names
	this.hdrFieldNames = ["From", "To", "Subject", "Message-ID", "Date", "X-Comment-To", "Path", "Organization", "Newsgroups", "X-FTN-PID", "Content-Type", "Content-Transfer-Encoding",
	                      "Expires", "Followup-To", "Lines", "MIME-Version", "Newsgroups", "NNTP-Posting-Client", "NNTP-Posting-Date", "NNTP-Posting-Host", "Reply-To", "Sender",
	                      "X-Accept-Language", "X-Admin", "X-Comments", "X-Complaints-To", "X-Mailer", "X-Newsreader", "X-Originating-Host", "X-Priority", "Xref", "X-Trace"];
}

// Connects to the server
//
// Return value: An object containing the following properties:
//               connected: Boolean - Whether or not the connect succeeded
//               connectedStr: The connect string received from the server
NNTPClient.prototype.Connect = function()
{
	var retObj = {
		connected: false,
		connectStr: ""
	};

	this.socket = new Socket();
	this.socket.debug = false;
	this.socket.bind(0, this.interfaceIP);
	retObj.connected = this.socket.connect(this.connectOptions.hostname, this.connectOptions.port);
	if (retObj.connected)
	{
		if (this.connectOptions.tls)
			this.socket.ssl_session = true;
		var conStr = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		var conStrIsString = (typeof(conStr) === "string");
		retObj.connected = (conStrIsString && conStr.indexOf("200") == 0);
		if (conStrIsString)
			retObj.connectStr = conStr;
	}
	return retObj;
}

// Returns the last error from the socket
NNTPClient.prototype.GetLastError = function()
{
	if (this.socket != null)
		return this.socket.last_error;
	else
		return -1;
}

// Disconnects from the server and destroyes the socket
NNTPClient.prototype.Disconnect = function()
{
	if (this.socket != null)
	{
		this.socket.sendline("QUIT");
		this.socket.close();
		delete this.socket;
		this.socket = null;
	}
}

// Authenticates the user with the server
//
// Return value: Boolean - Whether or not authentication succeeded
NNTPClient.prototype.Authenticate = function()
{
	var authSucceeded = false;
	this.socket.sendline("AUTHINFO USER " + this.connectOptions.username);
	var response = this.socket.recvline();
	if (this.connectOptions.password.length > 0)
	{
		this.socket.sendline("AUTHINFO PASS " + this.connectOptions.password);
		response = this.socket.recvline();
		authSucceeded = (typeof(response) === "string" && response.indexOf("281") == 0);
	}
	return authSucceeded;
}

// Gets an array of newsgroups from the server.
//
// Parameters:
//  pTimeoutOverride: Optional - An override for the time out (in seconds)
//
// Return value: An object containing the following properties:
//               succeeded: Boolean - Whether or not the operation succeeded
//               responseLine: The response line from the server
//               newsgroupArray: An array of objects containing the following properties:
//                               name: The name of the newsgroup
//                               desc: The newsgroup description
NNTPClient.prototype.GetNewsgroups = function(pTimeoutOverride)
{
	var retObj = {
		succeeded: false,
		responseLine: "",
		newsgroupArray: []
	};

	var LINE_FEED = "\x0a";
	//var numBytesSent = this.socket.send("LIST" + LINE_FEED);
	var sendSucceeded = this.socket.sendline("LIST NEWSGROUPS");
	//var numBytesSent = this.socket.send("LIST NEWSGROUPS" + LINE_FEED);
	//if (typeof(numBytesSent) === "number" && numBytesSent > 0)
	if (sendSucceeded)
	{
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("215") == 0);
		if (retObj.succeeded)
		{
			var timeout = (typeof(pTimeoutOverride) === "number" ? pTimeoutOverride : this.recvTimeoutSeconds);
			while (this.socket.is_connected)
			{
				var line = this.socket.recvline(this.recvBufSizeBytes, timeout);
				if (typeof(line) === "string" && line != ".")
				{
					// The server may use tabs as delimiters between the newsgroup name and description
					line = line.replace(/\t/g, " ");
					// Split on a space - The first will be the newsgroup name, and the 2nd will be its description
					var spaceIdx = line.indexOf(" ");
					if (spaceIdx > -1)
					{
						retObj.newsgroupArray.push({
							name: line.substr(0, spaceIdx),
							desc: truncsp(skipsp(line.substr(spaceIdx+1)))
						});
					}
					else
					{
						retObj.newsgroupArray.push({
							name: line,
							desc: ""
						});
					}
				}
				else
					break;
			}
		}
	}
	return retObj;
}

// Gets an array of new newsgroups since a given date & time from the server.
// See https://datatracker.ietf.org/doc/html/rfc3977#page-63 for more details.
//
// Parameters:
//  pDate: A date string in the format yymmdd or yyyymmdd
//  pTime: A time string in the format hhmmss
//  pIsGMT: Boolean - Whether or not the date & time are GMT. Defaults to false.
//  pTimeoutOverride: Optional - An override for the time out (in seconds)
//
// Return value: An object containing the following properties:
//               succeeded: Boolean - Whether or not the operation succeeded
//               responseLine: The response line from the server
//               newsgroupArray: An array of objects containing the following properties:
//                               name: The name of the newsgroup (string)
//                               highWatermark: The reported high water mark for the group (number)
//                               lowWatermark: The reported low water mark for the group (number)
//                               groupStatus: The current status of the group on this server (string):
//                                            "y": Posting is permitted
//                                            "n": Posting is not permitted
//                                            "m": Postings will be forwarded to the newsgroup moderator
NNTPClient.prototype.GetNewNewsgroups = function(pDate, pTime, pIsGMT, pTimeoutOverride)
{
	var retObj = {
		succeeded: false,
		responseLine: "",
		newsgroupArray: []
	};

	const isGMT = (typeof(pIsGMT) === "boolean" ? pIsGMT : false);

	var LINE_FEED = "\x0a";
	//var numBytesSent = this.socket.send("LIST" + LINE_FEED);
	// Information on the NEWGROUPS command:
	// https://datatracker.ietf.org/doc/html/rfc3977#page-63
	var command = format("NEWGROUPS %s %s", pDate, pTime);
	if (isGMT)
		command += " GMT";
	var sendSucceeded = this.socket.sendline(command);
	if (sendSucceeded)
	{
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("231") == 0);
		if (retObj.succeeded)
		{
			var timeout = (typeof(pTimeoutOverride) === "number" ? pTimeoutOverride : this.recvTimeoutSeconds);
			while (this.socket.is_connected)
			{
				var line = this.socket.recvline(this.recvBufSizeBytes, timeout);
				if (typeof(line) === "string" && line != ".")
				{
					// The server may use tabs as delimiters between the newsgroup name and description
					line = line.replace(/\t/g, " ");
					// Split on a space - The components are as follows:
					// Newsgroup name (no spaces)
					// The reported high water mark for the group
					// The reported low water mark for the group
					// The current status of the group on this server:
					//  "y": Posting is permitted
					//  "n": Posting is not permitted
					//  "m": Postings will be forwarded to the newsgroup moderator
					var lineComponents = line.split(" ");
					if (lineComponents.length == 0)
						continue;
					var grpName = "";
					var grpHighWatermark = 0;
					var grpLowWatermark = 0;
					var grpStatus = "y";
					var lineComponents = line.split(" ");
					if (lineComponents.length >= 1)
						grpName = lineComponents[0];
					if (lineComponents.length >= 2)
					{
						var numVal = parseInt(lineComponents[1]);
						if (!isNaN(numVal))
							grpHighWatermark = numVal;
					}
					if (lineComponents.length >= 3)
					{
						var numVal = parseInt(lineComponents[2]);
						if (!isNaN(numVal))
							grpLowWatermark = numVal;
					}
					if (lineComponents.length >= 4)
						grpStatus = lineComponents[3].toLowerCase();

					if (grpName.length > 0)
					{
						retObj.newsgroupArray.push({
							name: grpName,
							highWatermark: grpHighWatermark,
							lowWatermark: grpLowWatermark,
							groupStatus: grpStatus
						});
					}
				}
				else
					break;
			}
		}
	}
	return retObj;
}

// Gets an array of new newsgroups from the server since a local timestamp.
// See https://datatracker.ietf.org/doc/html/rfc3977#page-63 for more details.
//
// Parameters:
//  pTimestamp: A timestamp local to the BBS machine, in time_t format
//  pTimeoutOverride: Optional - An override for the time out (in seconds)
//
// Return value: An object containing the following properties:
//               succeeded: Boolean - Whether or not the operation succeeded
//               responseLine: The response line from the server
//               newsgroupArray: An array of objects containing the following properties:
//                               name: The name of the newsgroup (string)
//                               highWatermark: The reported high water mark for the group (number)
//                               lowWatermark: The reported low water mark for the group (number)
//                               groupStatus: The current status of the group on this server (string):
//                                            "y": Posting is permitted
//                                            "n": Posting is not permitted
//                                            "m": Postings will be forwarded to the newsgroup moderator
NNTPClient.prototype.GetNewNewsgroupsSinceLocalTimestamp = function(pTimestamp, pTimeoutOverride)
{
	var GMTTimestamp = localTimeTToGMT(pTimestamp);
	var dateStr = strftime("%Y%m%d", GMTTimestamp);
	var timeStr = strftime("%H%M%S", GMTTimestamp);
	return this.GetNewNewsgroups(dateStr, timeStr, true, pTimeoutOverride);
}

// Selects a newsgroup on the server
//
// Parameters:
//  pNewsgroupName: The name of the newsgroup to select
//
// Return value: An object containing the following properties:
//               succeeded: Boolean - Whether or not the selection succeeded
//               responseLine: The response line from the server
//               estNumArticles: The estimated number of articles. This will be -1 on failure.
//               firstArticleNum: The number of the first article.  This will be -1 on failure.
//               lastArticleNum: The number of the last article.  This will be -1 on failure.
NNTPClient.prototype.SelectNewsgroup = function(pNewsgroupName)
{
	var retObj = {
		succeeded: false,
		responseLine: "",
		estNumArticles: 0,
		firstArticleNum: -1,
		lastArticleNum: -1
	};

	if (this.socket.sendline("GROUP " + pNewsgroupName))
	{
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("211") == 0);
		/*
		Responses
		211 n f l s group selected
		n = estimated number of articles in group,
		f = first article number in the group,
		l = last article number in the group,
		s = name of the group.
		411 no such news group
		*/
		//211 50 5 164 Local.General group selected
		if (retObj.succeeded)
		{
			var numbers = retObj.responseLine.match(/\d+/g);
			if (Array.isArray(numbers))
			{
				var estNum = parseInt(numbers[1]);
				var firstNum = parseInt(numbers[2]);
				var lastNum = parseInt(numbers[3]);
				if (!isNaN(estNum) && !isNaN(firstNum) && !isNaN(lastNum))
				{
					retObj.estNumArticles = estNum;
					retObj.firstArticleNum = firstNum;
					retObj.lastArticleNum = lastNum;

					this.selectedNewsgroupName = pNewsgroupName;
				}
				else
					retObj.succeeded = false;
			}
			else
				retObj.succeeded = false;
		}
	}

	return retObj;
}

// Gets an article from the server, by article number.  Includes both the header and the body.
//
// Parameters:
//  pArticleNum: The number of the article to retrieve
//
// Return value: An object with the following propertieS:
//               succeeded: Boolean - Whether or not retrieving the article was successful
//               responseLine: The response line from the server
//               articleHdr: An object containing header properties & their values
//               articleBody: The body text of the article
NNTPClient.prototype.GetArticle = function(pArticleNum)
{
	var retObj = {
		succeeded: false,
		responseLine: "",
		articleHdr: {},
		articleBody: ""
	};

	if (this.socket.sendline("ARTICLE " + pArticleNum))
	{
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("220") == 0);
		if (retObj.succeeded)
		{
			// Get the article
			while (this.socket.is_connected)
			{
				var line = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
				if (typeof(line) === "string" && line != ".")
				{
					// Separate header elements into articleHdr, and only body text in bodyText
					var isHdrLine = false;
					for (var fI = 0; fI < this.hdrFieldNames.length; ++fI)
					{
						if (line.substr(0, this.hdrFieldNames[fI].length+2) == this.hdrFieldNames[fI] + ": ")
						{
							isHdrLine = true;
							retObj.articleHdr[this.hdrFieldNames[fI]] = line.substr(this.hdrFieldNames[fI].length+2);
							break;
						}
					}
					// Include arbitrary X- lines in the header object
					var results = line.match(/^X-[^:]+: /);
					if (Array.isArray(results) && results.length > 0)
					{
						isHdrLine = true;
						var colonIdx = results[0].indexOf(":");
						var hdrFieldName = (colonIdx > -1 ? results[0].substr(0, colonIdx) : results[0]);
						if (this.lowercaseHeaderFieldNames)
							hdrFieldName = hdrFieldName.toLowerCase();
						retObj.articleHdr[hdrFieldName] = line.substr(hdrFieldName.length+2);
					}

					// If not a header line, append to the article body
					if (!isHdrLine)
					{
						if (retObj.articleBody.length > 0)
							retObj.articleBody += "\r\n";
						retObj.articleBody += line;
					}
				}
				else
					break;
			}

			// If enabled, add the "to" field to the header if it's missing
			if (this.addHeaderToIfMissing)
			{
				var toFieldName = (this.lowercaseHeaderFieldNames ? "to" : "To");
				if (!retObj.articleHdr.hasOwnProperty(toFieldName))
					retObj.articleHdr[toFieldName] = "All";
			}

			// For the subject, decode certain encodings
			if (retObj.articleHdr.hasOwnProperty("subject"))
			{
				retObj.articleHdr.subject = retObj.articleHdr.subject.replace(/=\?UTF-8/g, "");
				retObj.articleHdr.subject = retObj.articleHdr.subject.replace(/\?Q/g, "");
				retObj.articleHdr.subject = retObj.articleHdr.subject.replace(/\?Re=3A_/g, "");
				retObj.articleHdr.subject = retObj.articleHdr.subject.replace(/\?=_/g, "");
				retObj.articleHdr.subject = decodeQuotedPrintable(retObj.articleHdr.subject);
			}
		}
	}

	// Line-wrap the article body, decode quoted-printable pieces, etc.
	if (retObj.articleBody.length > 0)
	{
		//retObj.articleBody = lineWrapArticleBody(retObj.articleBody);
		
		// From newsutil.js
		retObj.articleBody = decode_news_body(retObj.articleHdr, retObj.articleBody);

		// Decode quoted printable strings/characters in the article body
		retObj.articleBody = decodeQuotedPrintable(retObj.articleBody);
	
	}

	return retObj;
}

// Gets an article header from the server, by article number
//
// Parameters:
//  pArticleNum: The number of the article to retrieve
//
// Return value: An object with the following propertieS:
//               succeeded: Boolean - Whether or not retrieving the article was successful
//               responseLine: The response line from the server
//               articleHdr: An object containing the header fields & their values
NNTPClient.prototype.GetArticleHeader = function(pArticleNum)
{
	var retObj = {
		succeeded: false,
		responseLine: "",
		articleHdr: {}
	};

	if (this.socket.sendline("HEAD " + pArticleNum))
	{
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("221") == 0);
		if (retObj.succeeded)
		{
			while (this.socket.is_connected)
			{
				var line = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
				if (typeof(line) === "string" && line != ".")
				{
					var colonIdx = line.indexOf(":");
					if (colonIdx > -1)
					{
						var hdrFieldName = line.substr(0, colonIdx);
						if (this.lowercaseHeaderFieldNames)
							hdrFieldName = hdrFieldName.toLowerCase();
						retObj.articleHdr[hdrFieldName] = skipsp(line.substr(colonIdx+1));
					}
				}
				else
					break;
			}

			// If enabled, add the "to" field to the header if it's missing
			if (this.addHeaderToIfMissing)
			{
				var toFieldName = (this.lowercaseHeaderFieldNames ? "to" : "To");
				if (!retObj.articleHdr.hasOwnProperty(toFieldName))
					retObj.articleHdr[toFieldName] = "All";
			}

			// For the subject, decode certain encodings
			if (retObj.articleHdr.hasOwnProperty("subject"))
			{
				retObj.articleHdr.subject = retObj.articleHdr.subject.replace(/=\?UTF-8/gi, "");
				retObj.articleHdr.subject = retObj.articleHdr.subject.replace(/\?Q/gi, "");
				retObj.articleHdr.subject = retObj.articleHdr.subject.replace(/\?Re=3A_/gi, "");
				retObj.articleHdr.subject = retObj.articleHdr.subject.replace(/\?=_/g, "");
				retObj.articleHdr.subject = decodeQuotedPrintable(retObj.articleHdr.subject);
			}
		}
	}

	return retObj;
}

// Gets an article body from the server, by article number. Does not include the header.
//
// Parameters:
//  pArticleNum: The number of the article to retrieve
//  pArticleHdr: Optional - The article header
//
// Return value: An object with the following propertieS:
//               succeeded: Boolean - Whether or not retrieving the article was successful
//               responseLine: The response line from the server
//               body: The article body
NNTPClient.prototype.GetArticleBody = function(pArticleNum, pArticleHdr)
{
	var retObj = {
		succeeded: false,
		responseLine: "",
		body: ""
	};

	if (this.socket.sendline("BODY " + pArticleNum))
	{
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("222") == 0);
		if (retObj.succeeded)
		{
			while (this.socket.is_connected)
			{
				var line = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
				if (typeof(line) === "string" && line != ".")
				{
					if (retObj.body.length > 0)
						retObj.body += "\r\n";
					retObj.body += line;
				}
				else
					break;
			}

			// Decode certain encodings
			retObj.body = retObj.body.replace(/=\?UTF-8/gi, "");
			retObj.body = retObj.body.replace(/\?Q/gi, "");
			retObj.body = retObj.body.replace(/\?Re=3A_/gi, "");
			retObj.body = retObj.body.replace(/\?=_/g, "");

			// Line-wrap the article body for the user's terminal
			//retObj.body = lineWrapArticleBody(retObj.body);
			
			// From newsutil.js
			if (typeof(pArticleHdr) === "object")
				retObj.body = decode_news_body(pArticleHdr, retObj.body);

			// Decode quoted printable characters/strings in the body
			retObj.body = decodeQuotedPrintable(retObj.body);
		}
	}
	
	return retObj;
}

// Posts an article in the current selected newsgroup
//
// Parameters:
//  pTo: The username the message is written to
//  pSubject: The subject of the message
//  pBody: The message body text
//  pNewsgroupsOverride: Optional: A comma-separated list of newsgroup names for the message to be
//                       posted in.  Will be used in place of the current selected newsgroup if
//                       specified and has non-zero length.
//
// Return value: An object with the following propertieS:
//               succeeded: Boolean - Whether or not retrieving the article was successful
//               responseLine: The last response line from the server
//               body: The article body
NNTPClient.prototype.PostArticle = function(pTo, pSubject, pBody, pNewsgroupsOverride)
{
	var retObj = {
		succeeded: false,
		responseLine: ""
	};

	if (typeof(pBody) !== "string" || pBody.length == 0)
		return retObj;

	// TODO: Encode characters to quoted printable format

	if (this.socket.sendline("POST"))
	{
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("340") == 0);
		if (retObj.succeeded)
		{
			// Header lines
			var toName = (typeof(pTo) === "string" && pTo.length > 0 ? pTo : "All");
			var subject = (typeof(pSubject) === "string" && pSubject.length > 0 ? pSubject : "No Subject");
			var newsgroups = (typeof(pNewsgroupsOverride) === "string" && pNewsgroupsOverride.length > 0 ? pNewsgroupsOverride : this.selectedNewsgroupName);
			this.socket.sendline("From: " + user.handle); // TODO: Setting for this to be handle or real name
			this.socket.sendline("To: " + toName);
			this.socket.sendline("Subject: " + subject);
			//this.socket.sendline("Date: " + strftime("%a, %d %B %Y %H:%M:%S %z"));
			this.socket.sendline("Newsgroups: " + newsgroups);

			// Body text
			this.socket.sendline("");
			this.socket.sendline(pBody);
			//this.socket.sendline("\r\n.\r\n");
			this.socket.sendline("\r\n.");

			retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
			retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("240") == 0);
		}
	}

	return retObj;
}

// Tells the server to set its current article pointer to the next message in the newsgroup
//
// Return value: An object with the following propertieS:
//               succeeded: Boolean - Whether or not retrieving the article was successful
//               responseLine: The response line from the server
//               articleNum: The article number of the next article (if returned)
//               msgID: The message ID of the next article (if returned)
NNTPClient.prototype.Next = function()
{
	var retObj = {
		succeeded: false,
		articleNum: -1,
		msgID: -1,
		responseLine: ""
	};

	if (this.socket.sendline("NEXT"))
	{
		// 223 6 6 article retrieved - request text separately
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("223") == 0);
		if (retObj.succeeded)
		{
			var numbers = retObj.responseLine.match(/\d+/g);
			if (Array.isArray(numbers))
			{
				var articleNum = parseInt(numbers[1]);
				if (!isNaN(articleNum))
					retObj.articleNum = articleNum;
				var msgID = parseInt(numbers[2]);
				if (!isNaN(msgID))
					retObj.msgID = msgID;
			}
		}
	}

	return retObj;
}

// Tells the server to set its current article pointer to the last message in the newsgroup
//
// Return value: An object with the following propertieS:
//               succeeded: Boolean - Whether or not retrieving the article was successful
//               responseLine: The response line from the server
//               articleNum: The article number of the next article (if returned)
//               msgID: The message ID of the next article (if returned)
NNTPClient.prototype.Last = function()
{
	var retObj = {
		succeeded: false,
		articleNum: -1,
		msgID: -1,
		responseLine: ""
	};

	if (this.socket.sendline("LAST"))
	{
		// 223 5 5 article retrieved - request text separately
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("223") == 0);
		if (retObj.succeeded)
		{
			var numbers = retObj.responseLine.match(/\d+/g);
			if (Array.isArray(numbers))
			{
				var articleNum = parseInt(numbers[1]);
				if (!isNaN(articleNum))
					retObj.articleNum = articleNum;
				var msgID = parseInt(numbers[2]);
				if (!isNaN(msgID))
					retObj.msgID = msgID;
			}
		}
	}

	return retObj;
}

// Gets help from the news server
//
// Parameters:
//  pTimeoutOverride: Optional - An override for the time out (in seconds)
//
// Return value: An object containing the following properties:
//               succeeded: Boolean - Whether or not the operation succeeded
//               responseLine: The response line from the server
//               helpLines: An array of text lines containing the returned help text from the server
NNTPClient.prototype.Help = function(pTimeoutOverride)
{
	var retObj = {
		succeeded: false,
		responseLine: "",
		helpLines: []
	};

	var LINE_FEED = "\x0a";
	//var numBytesSent = this.socket.send("LIST" + LINE_FEED);
	// https://datatracker.ietf.org/doc/html/rfc3977#page-63
	var sendSucceeded = this.socket.sendline("HELP");
	//var numBytesSent = this.socket.send("LIST NEWSGROUPS" + LINE_FEED);
	//if (typeof(numBytesSent) === "number" && numBytesSent > 0)
	if (sendSucceeded)
	{
		retObj.responseLine = this.socket.recvline(this.recvBufSizeBytes, this.recvTimeoutSeconds);
		retObj.succeeded = (typeof(retObj.responseLine) === "string" && retObj.responseLine.indexOf("215") == 0);
		if (retObj.succeeded)
		{
			var timeout = (typeof(pTimeoutOverride) === "number" ? pTimeoutOverride : this.recvTimeoutSeconds);
			while (this.socket.is_connected)
			{
				var line = this.socket.recvline(this.recvBufSizeBytes, timeout);
				if (typeof(line) === "string" && line != ".")
				{
					// The server may use tabs as delimiters between the newsgroup name and description
					line = line.replace(/\t/g, " ");
					retObj.helpLines.push(line);
				}
				else
					break;
			}
		}
	}
	return retObj;
}


////////////////////////////////////////////////////////////////
// Misc. functions


// Decodes quoted-printable text in a string, and returns the new string.
//
// Parameters:
//  pStr: A string that may have quoted-printable text to decode
//
// Return value: The text with quoted-printable parts decoded
function decodeQuotedPrintable(pStr)
{
	if (typeof(pStr) !== "string")
		return "";

	// Remove soft line breaks ( = at end of line)
	var str = pStr.replace(/=\r?\n/g, "");

	// Replace =XX (hex) with the corresponding character
	str = str.replace(/=([A-Fa-f0-9]{2})/g, function (match, hex) {
		return String.fromCharCode(parseInt(hex, 16));
	});

	return str;
}

// Wraps text for a news article body
function lineWrapArticleBody(pBody)
{
	var wrappedBodyText = "";

	// Get information about the various 'sections'/paragraphs of the message
	var articleBodyArray = pBody.split("\r\n");
	var msgSections = getMsgSections(articleBodyArray);
	/*
	// Temporary
	if (user.is_sysop)
	{
		console.print("\x01n\r\n");
		console.print("articleBodyArray:\r\n");
		console.print("-----------------\r\n");
		for (var i = 0; i < articleBodyArray.length; ++i)
			console.print(i + ": " + articleBodyArray[i] + "\r\n");
		console.print("-----------------\r\n");
		console.print("Sections:\r\n");
		for (var i = 0; i < msgSections.length; ++i)
		{
			console.print(i + ":" + msgSections[i].linePrefix + ":\r\n");
			console.print(" Lines " + msgSections[i].begLineIdx + "-" + msgSections[i].endLineIdx + "\r\n");
		}
		console.pause();
	}
	// End Temporary
	*/
	//if (user.is_sysop) console.print("Article sections:\r\n"); // Temporary
	for (var i = 0; i < msgSections.length; ++i)
	{
		//var originalPrefix = msgSections[i].linePrefix;
		/*
		Section 2:
		begLineIdx: 2
		endLineIdx: 59
		linePrefix: > 
		*/
		if (msgSections[i].linePrefix.length > 0)
		{
			for (var lineIdx = msgSections[i].begLineIdx; lineIdx < msgSections[i].endLineIdx; ++lineIdx)
				wrappedBodyText += articleBodyArray[lineIdx] + "\r\n";
		}
		else
		{
			// No prefix for this section
			var currentLine = "";
			//if (user.is_sysop) console.print("Section " + +(i+1) + " (" + msgSections[i].begLineIdx + "-" + msgSections[i].endLineIdx + "):\r\n"); // Temporary
			for (var lineIdx = msgSections[i].begLineIdx; lineIdx < msgSections[i].endLineIdx; ++lineIdx)
			{
				//if (user.is_sysop) console.print(articleBodyArray[lineIdx] + "\r\n"); // Temporary
				var nextLineIsIndented = (lineIdx < msgSections[i].endLineIdx-1 && /^[ \t]/.test(articleBodyArray[lineIdx+1]) ? true : false);
				var lineLen = articleBodyArray[lineIdx].length;
				var lastCharInLine = (lineLen > 0 ? articleBodyArray[lineIdx][lineLen-1] : "");
				if ((articleBodyArray[lineIdx] == "" || lineLen < Math.floor(console.screen_columns/0.65) || nextLineIsIndented) && (lastCharInLine != "="))
				{
					currentLine += articleBodyArray[lineIdx];
					//wrappedBodyText += currentLine + "\r\n";
					// TODO: Wrap the current line of the section (using word_wrap)
					var currentLineWrappedArray = lfexpand(word_wrap(currentLine, console.screen_columns - 1)).split("\r\n");
					for (var wrappedArrayI = 0; wrappedArrayI < currentLineWrappedArray.length; ++wrappedArrayI)
						wrappedBodyText += currentLineWrappedArray[wrappedArrayI] + "\r\n";
					//if (user.is_sysop) console.print("currentLine (1)\r\n:" + currentLine + ":\r\n\x01p"); // Temporary
					currentLine = "";
				}
				else
				{
					// If the line ends with =, then append the line without the = and no space, as the word
					// continues on the next line. Otherwise, append the line with a space.
					if (articleBodyArray[lineIdx].lastIndexOf("=") == articleBodyArray[lineIdx].length-1)
						currentLine += articleBodyArray[lineIdx].substr(0, articleBodyArray[lineIdx].length-1);
					else
						currentLine += articleBodyArray[lineIdx] + " ";
				}
			}
			//if (user.is_sysop) console.crlf(); // Temporary
		}
		/*
		console.print("Section " + +(i+1) + ":\r\n");
		for (var prop in msgSections[i])
			console.print(prop + ": " + msgSections[i][prop] + "\r\n");
		*/
		console.crlf();
	}
	//if (user.is_sysop) console.pause(); // Temporary

	return wrappedBodyText;
}

// Parses a date from a message header into year, month, day, hour, minute, second,
// and time zone offset
function parseHdrDate(pArticleHdrOrDate)
{
	var retObj = {
		dayOfMonthNum: 1,
		monthNum: 1,
		year: 1970,
		hour: 1,
		min: 1,
		second: 0,
		timeZoneOffset: 0
	};

	var date = "";
	var paramType = typeof(pArticleHdrOrDate);
	if (paramType === "object" && pArticleHdrOrDate.hasOwnProperty("date"))
		date = pArticleHdrOrDate.date;
	else if (paramType == "string")
		date = pArticleHdrOrDate;

	// Try to convert the date string to the local BBS time zone, then parse it
	// https://www.w3schools.com/jsref/jsref_obj_date.asp
	var localDate = (new Date(date)).toLocaleString();
	// Wed, 12 Jun 2019 12:23:14 -0700
	// Thu, 24 Mar 2022 06:37:31 -0700 (PDT)
	var dateRegexp = /^([a-zA-Z]+), +([0-9]+) ([a-zA-Z]+) ([0-9]+) ([0-9]+):([0-9]+):([0-9]+) ([-+][0-9]+)/g;
	var matches = dateRegexp.exec(localDate); // hdr.date
	if (Array.isArray(matches) && matches.length == 9)
	{
		retObj.year = parseInt(matches[4]);
		retObj.monthNum = shortMonthNameToNum(matches[3]);
		retObj.dayOfMonthNum = parseInt(matches[2]);
		retObj.hour = parseInt(matches[5]);
		retObj.min = parseInt(matches[6]);
		retObj.second = parseInt(matches[7]);
		retObj.timeZoneOffset = parseInt(matches[8], 10);
	}
	else
	{
		// Thu Mar 31 23:17:10 2022
		// Sat Apr  9 19:07:21 2022
		dateRegexp = /^([a-zA-Z]+) ([a-zA-Z]+) +([0-9]+) ([0-9]+):([0-9]+):([0-9]+) ([0-9]+)/g;
		matches = dateRegexp.exec(localDate); // hdr.date
		if (Array.isArray(matches) && matches.length == 8)
		{
			retObj.year = parseInt(matches[7]);
			retObj.monthNum = shortMonthNameToNum(matches[2]);
			retObj.dayOfMonthNum = parseInt(matches[3]);
			retObj.hour = parseInt(matches[4]);
			retObj.min = parseInt(matches[5]);
			retObj.second = parseInt(matches[6]);
		}
		else
		{
			// Sat 9 Apr 19:07:21 2022
			dateRegexp = /^([a-zA-Z]+) +([0-9]+) ([a-zA-Z]+) ([0-9]+):([0-9]+):([0-9]+) ([0-9]+)/g;
			matches = dateRegexp.exec(localDate); // hdr.date
			if (Array.isArray(matches) && matches.length == 8)
			{
				retObj.year = parseInt(matches[7]);
				retObj.monthNum = shortMonthNameToNum(matches[3]);
				retObj.dayOfMonthNum = parseInt(matches[2]);
				retObj.hour = parseInt(matches[4]);
				retObj.min = parseInt(matches[5]);
				retObj.second = parseInt(matches[6]);
			}
		}
	}
	// TODO: 25 Sep 85 23:51:52 GMT

	if (retObj.dayOfMonthNum != null && retObj.year != null && retObj.monthNum != null && retObj.hour != null && retObj.min != null && retObj.second != null)
	{
		if (isNaN(retObj.year))
			retObj.year = 1970;
		if (isNaN(retObj.dayOfMonthNum))
			retObj.dayOfMonthNum = 1;
		if (isNaN(retObj.hour))
			retObj.hour = 1;
		if (isNaN(retObj.min))
			retObj.min = 1;
		if (isNaN(retObj.second))
			retObj.second = 0;
		if (retObj.timeZoneOffset == null || isNaN(retObj.timeZoneOffset))
			retObj.timeZoneOffset = 0;
	}

	return retObj;
}

function MakeSyncCompatibleMsgHdr(pArticleHdr)
{
	var hdr = {
		from: "",
		to: "",
		subject: ""
	};
	if (typeof(pArticleHdr) !== "object" || Array.isArray(pArticleHdr))
		return hdr;
	/*
	x-received: by 2002:a05:6830:99:b0:6b2:b82c:1417 with SMTP id    fri, 09 jun 2023 10: 45:24 -0700 (PDT) 2023 10: 45:23 -0700 (PDT)

	path: newsfeed.endofthelinebbs.com!weretis.net!feeder6.news.weretis.net!1.us.feeder.erje.net!feeder.erje.net!usenet.blueworldhosting.com!diablo1.usenet.blueworldhosting.com!peer01.iad!feed-me.highwinds-media.com!news.highwinds-media.com!news-out.google.com!nntp.google.com!postnews.google.com!google-groups.googlegroups.com!not-for-mail

	newsgroups: abg.allgemein

	in-reply-to: <2i2952Fi2hchU1@uni-berlin.de>

	injection-info: google-groups.googlegroups.com; posting-host=89.163.251.139; posting-account=tk_J2AoAAADQO1P1uFTmaXcOFym0S6jC

	nntp-posting-host: 89.163.251.139

	references: <40b8945b$1@news-kiste.de> <bGluc.138080$bz3.7074841@phobos.telenet-ops.be>

	user-agent: G2/1.0

	mime-version: 1.0

	message-id: <aaee041f-8418-4d0a-a5e6-60cb3e84e09an@googlegroups.com>

	injection-date: Fri, 09 Jun 2023 17:45:24 +0000

	content-type: text/plain; charset="UTF-8"

	content-transfer-encoding: quoted-printable

	x-received-bytes: 3683

	xref: newsfeed.endofthelinebbs.com abg.allgemein:1
	*/
	/*
	// Temporary
	console.clear("\x01n");
	for (var prop in pArticleHdr)
		console.print(prop + ": " + pArticleHdr[prop] + "\r\n");
	console.pause();
	// End Temporary
	*/
	// date: Fri, 9 Jun 2023 10:45:23 -0700 (PDT)
	var strProps = ["from", "to", "subject", "date"];
	for (var i = 0; i < strProps.length; ++i)
	{
		var prop = strProps[i];
		if (pArticleHdr.hasOwnProperty(prop) && typeof(pArticleHdr[prop]) === "string")
			hdr.from = pArticleHdr[prop];
	}
	hdr.is_utf8 = false;
	if (pArticleHdr.hasOwnProperty("content-type") && typeof(pArticleHdr["content-type"]) === "string")
	{
		var idx = pArticleHdr["content-type"].indexOf("charset=");
		if (idx > -1)
		{
			var charsetStr = pArticleHdr["content-type"].substr(idx+8).toUpperCase();
			hdr.is_utf8 =  (charsetStr == "\"UTF-8\"" || charsetStr == "UTF-8");
		}
	}
	hdr.attr = 0;
	hdr.auxattr = 0;
	hdr.netattr = NET_INTERNET;
	// TODO
	// x-received: by 2002:a05:6830:99:b0:6b2:b82c:1417 with SMTP id    fri, 09 jun 2023 10: 45:24 -0700 (PDT) 2023 10: 45:23 -0700 (PDT)
	// injection-date: Fri, 09 Jun 2023 17:45:24 +0000
	hdr.when_written_time = 0;        // Date/time (in time_t format)
	hdr.when_written_zone;            // Time zone (in SMB format)
	hdr.when_written_zone_offset = 0; // Time zone in minutes east of UTC
	hdr.when_imported_time;           // Date/time message was imported
	hdr.when_imported_zone;           // Time zone (in SMB format)
	hdr.when_imported_zone_offset;    // Time zone in minutes east of UTC
	return hdr;
}

/////////////////////////////////////////////////////////////////////
// Text wrapping functions (copied from SlyEdit_Misc.js and modified a bit)

// Wraps lines to a specified width, accounting for prefixes in front of sections of lines (i.e., for quoting).
// Normalizes some line prefixes (i.e., if some prefixes have multiple > characters separated by spaces, the
// spaces in between will be removed, etc.).
//
// Parameters:
//  pTextLineArray: An array of strings containing the text lines to be wrapped
//  pQuotePrefix: The line prefix to prepend to 'new' quote lines (ones without an existing prefix)
//  pIndentQuoteLines: Whether or not to indent the quote lines
//  pTrimSpacesFromQuoteLines: Whether or not to trim spaces from quote lines (for when people
//                             indent the first line of their reply, etc.).  Defaults to true.
//  pMaxWidth: The maximum width of the lines
//
// Return value: The wrapped text lines, as an array of strings
function wrapTextLinesForQuoting(pTextLines, pQuotePrefix, pIndentQuoteLines, pTrimSpacesFromQuoteLines, pMaxWidth)
{
	var quotePrefix = typeof(pQuotePrefix) === "string" ? pQuotePrefix : " > ";
	var maxLineWidth = typeof(pMaxWidth) === "number" && pMaxWidth > 0 ? pMaxWidth : console.screen_columns - 1;
	var indentQuoteLines = typeof(pIndentQuoteLines) === "boolean" ? pIndentQuoteLines : true;
	var trimSpacesFromQuoteLines = typeof(pTrimSpacesFromQuoteLines) === "boolean" ? pTrimSpacesFromQuoteLines : true;

	// Get information about the various 'sections'/paragraphs of the message
	var msgSections = getMsgSections(pTextLines);

	// Add an additional > character to lines with the prefix that have a >, and re-wrap the text lines
	// for the user's terminal width - 1
	var wrappedTextLines = [];
	for (var i = 0; i < msgSections.length; ++i)
	{
		var originalPrefix = msgSections[i].linePrefix;
		var previousSectionPrefix = i > 0 ? msgSections[i-1].linePrefix : "";
		var nextSectionPrefix = i < msgSections.length - 1 ? msgSections[i+1].linePrefix : "";

		// Make a copy of the section prefix. If there are any > characters with spaces (whitespace)
		// between them, get rid of the whitespace. Also, add another > to the end to indicate another
		// level of quoting.
		var thisSectionPrefix = msgSections[i].linePrefix;
		while (/>[\s]+>/.test(thisSectionPrefix))
			thisSectionPrefix = thisSectionPrefix.replace(/>[\s]+>/g, ">>");
		if (thisSectionPrefix.length > 0)
		{
			var charIdx = thisSectionPrefix.lastIndexOf(">");
			if (charIdx > -1)
			{
				// substrWithAttrCodes() is defined in dd_lightbar_menu.js
				var len = thisSectionPrefix.length - (charIdx+1);
				thisSectionPrefix = substrWithAttrCodes(thisSectionPrefix, 0, charIdx+1) + ">" + substrWithAttrCodes(thisSectionPrefix, charIdx+1, len);
			}
			if (!/ $/.test(thisSectionPrefix))
				thisSectionPrefix += " ";
		}
		else
		{
			// This section doesn't have a prefix, so it must be new unqoted text.  If this section
			// contains some non-blank lines, then set the quote prefix to the passed-in/default prefix
			var hasNonBlankLines = false;
			for (var textLineIdx = msgSections[i].begLineIdx; textLineIdx <= msgSections[i].endLineIdx && !hasNonBlankLines; ++textLineIdx)
				hasNonBlankLines = (console.strlen(pTextLines[textLineIdx].trim()) > 0);
			if (hasNonBlankLines)
				thisSectionPrefix = quotePrefix;
		}
		// If there is a prefix, then make sure it's indended or not indented, according to the parameter passed in
		if (thisSectionPrefix.length > 0)
		{
			var firstPrefixChar = thisSectionPrefix.charAt(0);
			if (indentQuoteLines)
			{
				if (firstPrefixChar != " ")
					thisSectionPrefix = " " + thisSectionPrefix;
			}
			else
				thisSectionPrefix = thisSectionPrefix.replace(/^\s+/, ""); // Remove any leading whitespace
		}

		// Build a long string containing the current section's text
		var arbitaryLongLineLen = 120; // An arbitrary line length (for determining when to add a CRLF)
		var mostOfConsoleWidth = Math.floor(console.screen_columns * 0.85); // For checking when to add a CRLF
		var halfConsoleWidth = Math.floor(console.screen_columns * 0.5); // For checking when to add a CRLF
		var sectionText = "";
		for (var textLineIdx = msgSections[i].begLineIdx; textLineIdx <= msgSections[i].endLineIdx; ++textLineIdx)
		{
			// If the line is a blank line, then count how many blank lines there are in a row
			// and append that many CRLF strings to the section text
			if (stringIsEmptyOrOnlyWhitespace(pTextLines[textLineIdx]))
				sectionText += "\r\n";
			else
			{
				var thisLineTrimmed = pTextLines[textLineIdx].trim();
				if ((nextSectionPrefix != "" && thisLineTrimmed == nextSectionPrefix.trim()) || (previousSectionPrefix != "" && thisLineTrimmed == previousSectionPrefix.trim()))
					sectionText += "\r\n";
				else
				{
					// Trim leading & trailing whitespace from the text & append it to the section text
					//sectionText += pTextLines[textLineIdx].substr(originalPrefix.length).trim() + " ";
					// substrWithAttrCodes() is defined in dd_lightbar_menu.js
					var len = pTextLines[textLineIdx].length - originalPrefix.length;
					//sectionText += substrWithAttrCodes(pTextLines[textLineIdx], originalPrefix.length, len).trim() + " ";
					var currentLineText = substrWithAttrCodes(pTextLines[textLineIdx], originalPrefix.length, len);
					if (trimSpacesFromQuoteLines)
						currentLineText = currentLineText.trim();
					//sectionText += currentLineText + " ";
					sectionText += currentLineText;
					// If the next line isn't blank and the current line is less than half of the
					// terminal width, append a \r\n to the end (the line may be this short on purpose)
					var numLinesInSection = msgSections[i].endLineIdx - msgSections[i].begLineIdx + 1;
					// See if the next line is blank or is a tear/origin line (starting with "--- " or "Origin").
					// In these situations, we want (or may want) to add a hard newline (CRLF: \r\n) at the end.
					var nextLineIsBlank = false;
					var nextLineIsOriginOrTearLine = false;
					if (textLineIdx < msgSections[i].endLineIdx)
					{
						var nextLineTrimmed = pTextLines[textLineIdx+1].trim();
						nextLineIsBlank = (console.strlen(nextLineTrimmed) == 0);
						if (!nextLineIsBlank)
							nextLineIsOriginOrTearLine = msgLineIsTearLineOrOriginLine(nextLineTrimmed);
					}
					// Get the length of the current line (if it needs wrapping, then wrap it & get the
					// length of the last line after wrapping), so that if that length is short, we might
					// put a CRLF at the end to signify the end of the paragraph/section
					var lineLastLength = 0;
					var currentLineScreenLen = console.strlen(pTextLines[textLineIdx]);
					if (currentLineScreenLen < console.screen_columns - 1)
						lineLastLength = currentLineScreenLen;
					else
					{
						var currentLine = pTextLines[textLineIdx];
						if (trimSpacesFromQuoteLines)
							currentLine = currentLine.trim();
						var paragraphLines = lfexpand(word_wrap(currentLine, console.screen_columns-1, null, true)).split("\r\n");
						paragraphLines.pop(); // There will be an extra empty line at the end; remove it
						if (paragraphLines.length > 0)
							lineLastLength = console.strlen(paragraphLines[paragraphLines.length-1]);
					}
					// Put a CRLF at the end in certain conditions
					var lineScreenLen = console.strlen(pTextLines[textLineIdx]);
					if (nextLineIsOriginOrTearLine || nextLineIsBlank)
						sectionText += "\r\n";
					// Append a CRLF if the line isn't blank and its length is less than 85% of the user's terminal width
					// ..and if the text line length is originally longer than an arbitrary length (bit arbitrary, but if a line is that long, then
					// it's probably its own paragraph
					else if (lineScreenLen > arbitaryLongLineLen && numLinesInSection > 1 && !nextLineIsBlank && lineLastLength <= mostOfConsoleWidth)
						sectionText += "\r\n";
					else if (lineScreenLen <= halfConsoleWidth && !nextLineIsBlank)
						sectionText += "\r\n";
					else
						sectionText += " ";
				}
			}
		}
		// Remove the trailing space from the end, and wrap the section's text according to the length
		// with the current prefix, then append the re-wrapped text lines to wrappedTextLines
		sectionText = sectionText.replace(/ $/, "");
		var textWrapLen = maxLineWidth - thisSectionPrefix.length;
		var msgSectionLines = lfexpand(word_wrap(sectionText, textWrapLen, null, true)).split("\r\n");
		msgSectionLines.pop(); // There will be an extra empty line at the end; remove it
		for (var wrappedSectionIdx = 0; wrappedSectionIdx < msgSectionLines.length; ++wrappedSectionIdx)
		{
			// Prepend the text line with the section prefix only if the line isn't blank
			if (console.strlen(msgSectionLines[wrappedSectionIdx]) > 0)
				wrappedTextLines.push(thisSectionPrefix + msgSectionLines[wrappedSectionIdx]);
			else
				wrappedTextLines.push(msgSectionLines[wrappedSectionIdx]);
		}
	}
	return wrappedTextLines;
}
// Gets an an aray of message sections from an array of text lines.
// This was originally a helper for wrapTextLinesForQuoting() in
// SlyEdit_Misc.js
//
// Parameters:
//  pTextLines: An array of strings
//
// Return value: An array of MsgSection objects representing the different sections of the message with various
//               quote prefixes
function getMsgSections(pTextLines)
{
	var msgSections = [];

	var lastQuotePrefix = findPatternedQuotePrefix(pTextLines[0]);
	if (user.is_sysop) console.print("\x01n\r\n"); // Temporary
	if (user.is_sysop) console.print("Quote prefix 0:" + lastQuotePrefix + ":\r\n"); // Temporary
	var startLineIdx = 0;
	var lastLineIdx = 0;
	for (var i = 1; i < pTextLines.length; ++i)
	{
		var quotePrefix = findPatternedQuotePrefix(pTextLines[i]);
		if (user.is_sysop) console.print("Quote prefix " + i + ":" + quotePrefix + ":\r\n"); // Temporary
		var lineIsOnlyPrefix = pTextLines[i] == quotePrefix;
		quotePrefix = quotePrefix.replace(/\s+$/, "");
		if (lineIsOnlyPrefix)
			quotePrefix = "";
		//console.print((i+1) + ": Quote prefix length: " + quotePrefix.length + "\r\n"); // Temproary
		//if (quotePrefix.length == 0)
		//	continue;

		//console.print((i+1) + ":" + quotePrefix + ":\r\n");
		//console.print("Line is only prefix: " + lineIsOnlyPrefix + "\r\n");
		//console.crlf();
		//console.print((i+1) + ": Quote prefix:" + quotePrefix + ":, last:" + lastQuotePrefix + ":\r\n"); // Temporary
		if (quotePrefix != lastQuotePrefix)
		{
			if (lastQuotePrefix.length > 0)
				msgSections.push(new MsgSection(startLineIdx, i-1, lastQuotePrefix));
			startLineIdx = i;
		}

		lastQuotePrefix = quotePrefix;
	}
	if (user.is_sysop) console.pause(); // Temporary
	if (msgSections.length > 0)
	{
		// Add the message sections that are missing (no prefix)
		var lineStartIdx = 0;
		var lastEndIdxSeen = 0;
		var numSections = msgSections.length;
		for (var i = 0; i < numSections; ++i)
		{
			if (msgSections[i].begLineIdx > lineStartIdx)
				msgSections.push(new MsgSection(lineStartIdx, msgSections[i].begLineIdx-1, ""));
			lineStartIdx = msgSections[i].endLineIdx + 1;
			lastEndIdxSeen = msgSections[i].endLineIdx;
		}
		if (lastEndIdxSeen+1 < pTextLines.length - 1)
			msgSections.push(new MsgSection(lastEndIdxSeen+1, pTextLines.length - 1, ""));
		// Sort the message sections (by beginning line index)
		msgSections.sort(function(obj1, obj2) {
			if (obj1.begLineIdx < obj2.begLineIdx)
				return -1;
			else if (obj1.begLineIdx == obj2.begLineIdx)
				return 0;
			else if (obj1.begLineIdx > obj2.begLineIdx)
				return 1;
		});
	}
	else // There are no message sections; add one for the whole message with no prefix
		msgSections.push(new MsgSection(0, pTextLines.length - 1, ""));

	return msgSections;
}
// Helper for wrapTextLinesForQuoting(): Creates an object containing information aboug a message section (with or without a common line prefix)
function MsgSection(pBegLineIdx, pEndLineIdx, pLinePrefix)
{
	this.begLineIdx = pBegLineIdx;
	this.endLineIdx = pEndLineIdx;
	this.linePrefix = pLinePrefix;
}
// Looks for a quote prefix with a typical pattern at the start of a string.
// Returns it if found; returns an empty string if not found.
//
// Parameters:
//  pStr: A string to search
//
// Return value: The string's quote prefix, if found, or an empty string if not found
function findPatternedQuotePrefix(pStr)
{
	var strPrefix = "";
	// See if there is a quote prefix with a typical pattern (possibly a space with
	// possibly some characters followed by a > and a space).  Make sure it only gets
	// the first instance of a >, in case there are more.  Look for the first > and
	// get a substring with just that one and try to match it with a regex of a pattern
	// followed by >
	// First, look for just alternating spaces followed by > at the start of the string.
	//var prefixMatches = pStr.match(/^( *>)+ */);
	var prefixMatches = pStr.match(/^( *\S*>)+ */); // Possible whitespace followed by possible-non-whitespace
	if (Array.isArray(prefixMatches) && prefixMatches.length > 0)
	{
		if (pStr.indexOf(prefixMatches[0]) == 0) // >= 0
			strPrefix = prefixMatches[0];
	}
	else
	{
		// Alternating spaces and > at the start weren't found.  Look for the first >
		// and if found, see if it has any spaces & characters before it
		var GTIdx = pStr.indexOf("> "); // Look for the first >
		if (GTIdx >= 0)
		{
			///*var */prefixMatches = pStr.substr(0, GTIdx+2).match(/^ *[\d\w\W]*> /i);
			// substrWithAttrCodes() is defined in dd_lightbar_menu.js
			var len = pStr.length - (GTIdx+2);
			prefixMatches = substrWithAttrCodes(pStr, 0, GTIdx+2, len).match(/^ *[\d\w\W]*> /i);
			if (Array.isArray(prefixMatches) && prefixMatches.length > 0)
			{
				if (pStr.indexOf(prefixMatches[0]) == 0) // >= 0
					strPrefix = prefixMatches[0];
			}
		}
	}
	// If the prefix is over a certain length, then perhaps it's not actually a valid
	// prefix
	if (strPrefix.length > Math.floor(console.screen_columns/2))
		strPrefix = "";
	return strPrefix;
}
// Returns whether a text line is a tear line ("---") or an origin line
function msgLineIsTearLineOrOriginLine(pTextLine)
{
	return (pTextLine == "---" || pTextLine.indexOf("--- ") == 0 || pTextLine.indexOf(" --- ") == 0 || pTextLine.indexOf(" * Origin: ") == 0);
}
// Returns whether a string is empty or only whitespace
function stringIsEmptyOrOnlyWhitespace(pString)
{
	if (typeof(pString) !== "string")
		return false;
	return (pString.length == 0 || /^\s+$/.test(pString));
}

// Converts a local timestamp (in time_t format) to GMT (UTC) time
//
// Parameters:
//  pTime: A local timestamp (in time_t format)
//
// Return value: The time_t value converted to GMT (UTC)
function localTimeTToGMT(pTime)
{
	// system.tz_offset is the BBS's local timezone offset (in minutes) from UTC.
	// Negative values represent zones west of UTC; positive values represent
	// zones east of UTC.
	// Multiplying the resulting value by -1 in order to get GMT time by
	// adding the offset to pTime
	var localOffsetInSeconds = -(system.tz_offset * 60); // 60 seconds per minute
	return pTime + localOffsetInSeconds;
}

////////////////////////////////////////////////////////////////

// Leave this as the last line; maybe this is used as a lib
this;