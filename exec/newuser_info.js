// New User Information/Help module
// for Synchronet Terminal Server v3.21+

require("sbbsdefs.js", "P_NOABORT");
require("gettext.js", "gettext");

"use strict";

bbs.menu("../sbbs", P_NOABORT | P_NOERROR);
bbs.menu("../system", P_NOABORT | P_NOERROR);
bbs.menu("../newuser", P_NOABORT | P_NOERROR);

if (Object.getOwnPropertyNames(xtrn_area.editor).length && (system.newuser_questions & UQ_XEDIT) && bbs.text(bbs.text.UseExternalEditorQ)) {
	console.cond_blankline();
	if (console.yesno(bbs.text(bbs.text.UseExternalEditorQ))) {
		if (!bbs.select_editor()) {
			console.print(gettext("Sorry, no external editors are available to you"))
			console.newline();
		}
	} else
		user.editor = "";
}

if ((system.newuser_questions & UQ_CMDSHELL) && bbs.text(bbs.text.CommandShellHeading)) {
	bbs.select_shell();
}
