// MailSig.to_terminal, from_terminal and lossy: the mail signature is UTF-8,
// a CP437 terminal sees and edits it converted

load(js.exec_dir + "kit.js");
var MailSig = Kit.lib("mailsig");

var ascii = "Best,\nRob (555) 123-4567";
var box = "\xE2\x94\x80\xE2\x94\x80 Rob \xE2\x94\x80\xE2\x94\x80";	// U+2500 x2
var box437 = "\xC4\xC4 Rob \xC4\xC4";
var accent = "Caf\xC3\xA9";	// U+00E9
var accent437 = "Caf\x82";
var emoji = "hi \xF0\x9F\x98\x80";	// U+1F600

// ASCII is unchanged both ways, for both terminal kinds
Kit.eq(MailSig.to_terminal(ascii, true), ascii);
Kit.eq(MailSig.to_terminal(ascii, false), ascii);
Kit.eq(MailSig.from_terminal(ascii, true), ascii);
Kit.eq(MailSig.from_terminal(ascii, false), ascii);

// UTF-8 terminal: unchanged
Kit.eq(MailSig.to_terminal(box, true), box);
Kit.eq(MailSig.from_terminal(box, true), box);

// CP437 terminal: box drawing and accents convert to their CP437 bytes
Kit.eq(MailSig.to_terminal(box, false), box437);
Kit.eq(MailSig.from_terminal(box437, false), box);
Kit.eq(MailSig.to_terminal(accent, false), accent437);
Kit.eq(MailSig.from_terminal(accent437, false), accent);

// lossy: only when a CP437 terminal could not round-trip the text
Kit.eq(MailSig.lossy(ascii), false);
Kit.eq(MailSig.lossy(box), false);
Kit.eq(MailSig.lossy(accent), false);
Kit.eq(MailSig.lossy(""), false);
Kit.eq(MailSig.lossy(emoji), true);
Kit.eq(MailSig.lossy(accent + "\n" + emoji), true);

// from_editor: what was typed in the terminal's editor becomes plain text:
// Ctrl-A codes and ANSI sequences are removed, and that is reported
Kit.eq(MailSig.from_editor(ascii, true), { text: ascii, stripped: false }, "plain text is kept");
Kit.eq(MailSig.from_editor("\x01h\x01rRed\x01n Rob", false), { text: "Red Rob", stripped: true }, "Ctrl-A codes removed");
Kit.eq(MailSig.from_editor("\x1b[1;31mRed\x1b[0m Rob", true), { text: "Red Rob", stripped: true }, "ANSI sequences removed");
Kit.eq(MailSig.from_editor(box437, false), { text: box, stripped: false }, "CP437 from the terminal becomes UTF-8");
Kit.eq(MailSig.from_editor(box, true), { text: box, stripped: false }, "UTF-8 from a UTF-8 terminal is kept");
Kit.eq(MailSig.from_editor("", false), { text: "", stripped: false }, "empty stays empty");
