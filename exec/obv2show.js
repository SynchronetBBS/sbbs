// obv2show.js

// Displays an Oblivion/2 text/ANSI menu file, expanding MCI codes

// usage (from Baja):
//						set str <filename>
//						exec ?obv2show %s

f = new File(argv[0]);
if(!f.open("r")) {
	print("Error " + f.error + " opening " + f.name);
	exit();
}
text=f.readAll();
f.close();

for(i in text) {
	t=text[i];

	if(t.indexOf('%')>=0) {

		/* search and replace "standard codes" */

		/* User properties */
		t=t.replace(/%UN/g,user.alias);
		t=t.replace(/%RN/g,user.name);
		t=t.replace(/%PN/g,user.phone);
		t=t.replace(/%AD/g,user.address);
		t=t.replace(/%LO/g,user.location);
		t=t.replace(/%UC/g,user.note);
		t=t.replace(/%UL/g,user.security.level);
		t=t.replace(/%FL/g,user.security.level);	// File Level
		t=t.replace(/%FP/g,user.security.credits);	// File Points
		t=t.replace(/%UK/g,Math.floor(user.stats.bytes_uploaded/1024));	// Uploaded K
		t=t.replace(/%DK/g,Math.floor(user.stats.bytes_downloaded/1024));	// Downloaded K
		t=t.replace(/%UP/g,user.stats.files_uploaded);		// Uploads
		t=t.replace(/%DN/g,user.stats.files_downloaded);	// Downloads
		t=t.replace(/%PS/g,user.stats.total_posts);			// Posts
		t=t.replace(/%CS/g,user.stats.total_logons);		// Calls

		t=t.replace(/%TL/g,system.secondstr(bbs.time_left).slice(1));	// Time Left

		t=t.replace(/%NF/g,system.stats.files_uploaded_today);	// New Files
		t=t.replace(/%NP/g,system.stats.messages_posted_today);	// New Posts

		t=t.replace(/%CR/g,user.connection);				// Connect Rate


		t=t.replace(/%TT/g,"@TPERD@");						// Daily Time Limit

		t=t.replace(/%LC/g,system.lastuseron);				// Last Caller

		t=t.replace(/%TC/g,system.stats.total_logons);		// Total Calls
		t=t.replace(/%CT/g,system.stats.logons_today);		// Calls Today
		t=t.replace(/%FT/g,system.stats.files_uploaded_today);
		t=t.replace(/%PT/g,system.stats.messages_posted_today);
		t=t.replace(/%NT/g,system.stats.new_users_today);
		t=t.replace(/%UU/g,user.number);

		t=t.replace(/%BN/g,system.name);		// Board Name
		t=t.replace(/%SN/g,system.operator);	// Sysop Name
		t=t.replace(/%DT/g,system.datestr());	// Date
		t=t.replace(/%TM/g,system.timestr());	// Time

	//          SS Current Status Screen Library Name
	//          MS Current Menu Library Name

		t=t.replace(/%PC/g,Math.floor(user.stats.total_logons/user.stats.total_posts));	// Post/Call Ratio

		t=t.replace(/%NR/g,Math.floor(user.stats.files_downloaded/user.stats.files_uploaded));	// U/D Ratio
		t=t.replace(/%KR/g,Math.floor(user.stats.bytes_downloaded/user.stats.bytes_uploaded));	// U/D K Ratio

		t=t.replace(/%LD/g,user.stats.laston_date); // Last On Date


		t=t.replace(/%CA/g,bbs.curgrp);				// Current Area
		t=t.replace(/%CB/g,bbs.cursub);				// Current Base

	//          DU Days until expiration

		t=t.replace(/%NN/g,bbs.node_num);			// Node Number

		t=t.replace(/%VN/g,system.version);			// Version Number
		t=t.replace(/%VD/g,system.compiled_when);	// Version Date

		t=t.replace(/%TF/g,system.stats.total_files);		// Total Files
		t=t.replace(/%TP/g,system.stats.total_messsages);	// Total Posts

		t=t.replace(/%CF/g,bbs.curlib);				// Current File Conference
		t=t.replace(/%CM/g,bbs.curgrp);				// Current Message Conference

	//          CE Message # (Only activated when used with the Nx message newscanning commands)


		/*******************/
		/* Console control */
		/*******************/

		if(t.indexOf("%PA")>=0) {
			t=t.replace(/%PA/g,"");					// Pauses the screen
			console.pause();
		}
		if(t.indexOf("%PF")>=0) {
			t=t.replace(/%PF/g,"");					// Turns Screen Pausing Off
			bbs.sys_status|=SS_PAUSEOFF;
		}
		if(t.indexOf("%PO")>=0) {
			t=t.replace(/%PO/g,"");					// Turns Screen Pausing Back On 
			bbs.sys_status&=~SS_PAUSEOFF;
		}
		if(t.indexOf("%UA")>=0) {
			t=t.replace(/%UA/g,"");					// Makes anything below it unabortable
			bbs.sys_status&=~SS_ABORT;
		}

	}

	/* print line */
	console.putmsg(t + "\r\n");
}
