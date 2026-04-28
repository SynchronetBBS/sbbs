// md2asc.js

// Convert Markdown to plain-text with (optional) Synchronet attribute (Ctrl-A) codes

function md2asc(buf, mono)
{
	var NORMAL			="\x01N";
	var HEADING1		="\x01H\x01Y";
	var HEADING2		="\x01H\x01C";
	var HEADING3		="\x01H\x01M";
	var HEADING4		="\x01H\x01G";
	var HEADING5		="\x01H\x01B";
	var HEADING6		="\x01H\x01R";
	var BOLD			="\x01H\x01W";
	var ITALIC			="\x01N\x01C";
	var BOLD_ITALIC		="\x01H\x01C";
	var STRIKE			="\x01N\x01K\x017";
	var CODE_SPAN		="\x01H\x01G";
	var CODE_BLOCK		="\x01H\x01G";
	var BLOCKQUOTE		="\x01N\x01C";
	var LINK_TEXT		="\x01H\x01C";
	var LINK_URL		="\x01N\x01B";
	var HRULE			="\x01N\x01B";
	var LIST_BULLET		="\x01H\x01W";

	if(buf == undefined)
		return buf;
	if(mono === undefined)
		mono = false;
	if(mono) {
		NORMAL			="";
		HEADING1		="";
		HEADING2		="";
		HEADING3		="";
		HEADING4		="";
		HEADING5		="";
		HEADING6		="";
		BOLD			="";
		ITALIC			="";
		BOLD_ITALIC		="";
		STRIKE			="";
		CODE_SPAN		="";
		CODE_BLOCK		="";
		BLOCKQUOTE		="";
		LINK_TEXT		="";
		LINK_URL		="";
		HRULE			="";
		LIST_BULLET		="";
	}

	var lines = buf.replace(/\r\n/g, "\n").replace(/\r/g, "\n").split("\n");
	var out = [];
	var i;
	var in_code_block = false;
	var code_block_buf = [];

	for(i = 0; i < lines.length; i++) {
		var line = lines[i];

		// Fenced code blocks (``` or ~~~)
		if(!in_code_block && line.match(/^(`{3,}|~{3,})/)) {
			in_code_block = true;
			code_block_buf = [];
			continue;
		}
		if(in_code_block) {
			if(line.match(/^(`{3,}|~{3,})\s*$/)) {
				in_code_block = false;
				for(var c = 0; c < code_block_buf.length; c++)
					out.push("    " + CODE_BLOCK + code_block_buf[c] + NORMAL);
				continue;
			}
			code_block_buf.push(line);
			continue;
		}

		// Handle inline HTML (outside of code blocks)
		// <summary> -> bold text, <details> -> strip, <br> -> blank line
		line = line.replace(/<summary>([^<]*)<\/summary>/gi, "**$1**");
		line = line.replace(/<\/?details[^>]*>/gi, "");
		line = line.replace(/<br\s*\/?>/gi, "");
		// Strip other HTML tags (<div>, <span>, <kbd>, <p>, etc.)
		line = line.replace(/<\/?[a-z][a-z0-9]*(?:\s[^>]*)?\/?>/gi, "");

		// Blank line
		if(line.match(/^\s*$/)) {
			out.push("");
			continue;
		}

		// Horizontal rule (---, ***, ___, or more, optionally with spaces)
		if(line.match(/^ {0,3}([-*_] *){3,}\s*$/)) {
			out.push(HRULE + "----------------------------------------" + NORMAL);
			continue;
		}

		// ATX headings (# through ######)
		var heading_match = line.match(/^(#{1,6})\s+(.+?)(?:\s+#+)?\s*$/);
		if(heading_match) {
			var level = heading_match[1].length;
			var text = md2asc_inline(heading_match[2], mono, NORMAL, BOLD, ITALIC, BOLD_ITALIC,
				STRIKE, CODE_SPAN, LINK_TEXT, LINK_URL);
			var heading_attrs = [HEADING1, HEADING2, HEADING3, HEADING4, HEADING5, HEADING6];
			var heading_decor = ["*** ", "%%% ", "--- ", "-=< ", "... ", "___ "];
			var heading_close = [" ***", " %%%", " ---", " >=-", " ...", " ___"];
			out.push("");
			out.push(heading_attrs[level - 1] + heading_decor[level - 1]
				+ text + heading_close[level - 1] + NORMAL);
			if(level <= 2)
				out.push("");
			continue;
		}

		// Setext headings (underline with === or ---)
		if(i + 1 < lines.length) {
			var next = lines[i + 1];
			if(next.match(/^={3,}\s*$/)) {
				var text = md2asc_inline(line, mono, NORMAL, BOLD, ITALIC, BOLD_ITALIC,
					STRIKE, CODE_SPAN, LINK_TEXT, LINK_URL);
				out.push("");
				out.push(HEADING1 + "*** " + text + " ***" + NORMAL);
				out.push("");
				i++;
				continue;
			}
			if(next.match(/^-{3,}\s*$/) && !line.match(/^\s*$/)) {
				var text = md2asc_inline(line, mono, NORMAL, BOLD, ITALIC, BOLD_ITALIC,
					STRIKE, CODE_SPAN, LINK_TEXT, LINK_URL);
				out.push("");
				out.push(HEADING2 + "%%% " + text + " %%%" + NORMAL);
				out.push("");
				i++;
				continue;
			}
		}

		// Blockquote
		var bq_match = line.match(/^(>\s*)+(.*)$/);
		if(bq_match) {
			var depth = line.match(/>/g).length;
			var prefix = "";
			for(var d = 0; d < depth; d++)
				prefix += "| ";
			var bq_text = bq_match[2];
			bq_text = md2asc_inline(bq_text, mono, NORMAL, BOLD, ITALIC, BOLD_ITALIC,
				STRIKE, CODE_SPAN, LINK_TEXT, LINK_URL);
			out.push(BLOCKQUOTE + prefix + bq_text + NORMAL);
			continue;
		}

		// Unordered list item (-, *, +)
		var ul_match = line.match(/^(\s*)([-*+])\s+(.*)$/);
		if(ul_match) {
			var indent = ul_match[1].length;
			var pad = "";
			for(var s = 0; s < indent; s++)
				pad += " ";
			var item_text = md2asc_inline(ul_match[3], mono, NORMAL, BOLD, ITALIC,
				BOLD_ITALIC, STRIKE, CODE_SPAN, LINK_TEXT, LINK_URL);
			out.push(pad + " " + LIST_BULLET + "o " + NORMAL + item_text);
			continue;
		}

		// Ordered list item
		var ol_match = line.match(/^(\s*)(\d+)[.)]\s+(.*)$/);
		if(ol_match) {
			var indent = ol_match[1].length;
			var pad = "";
			for(var s = 0; s < indent; s++)
				pad += " ";
			var item_text = md2asc_inline(ol_match[3], mono, NORMAL, BOLD, ITALIC,
				BOLD_ITALIC, STRIKE, CODE_SPAN, LINK_TEXT, LINK_URL);
			out.push(pad + LIST_BULLET + ol_match[2] + ". " + NORMAL + item_text);
			continue;
		}

		// Indented code block (4 spaces or 1 tab)
		if(line.match(/^(    |\t)/) && i > 0 && (out.length == 0 || out[out.length - 1] == ""
			|| out[out.length - 1].indexOf(CODE_BLOCK) == 0
			|| (out[out.length - 1].indexOf("    " + CODE_BLOCK) == 0))) {
			var code_line = line.replace(/^(    |\t)/, "");
			out.push("    " + CODE_BLOCK + code_line + NORMAL);
			continue;
		}

		// Normal paragraph text
		line = md2asc_inline(line, mono, NORMAL, BOLD, ITALIC, BOLD_ITALIC,
			STRIKE, CODE_SPAN, LINK_TEXT, LINK_URL);
		out.push(line);
	}

	// Handle unterminated code block
	if(in_code_block) {
		for(var c = 0; c < code_block_buf.length; c++)
			out.push("    " + CODE_BLOCK + code_block_buf[c] + NORMAL);
	}

	var result = out.join("\r\n");

	// Clean up excessive blank lines (3+ -> 2)
	result = result.replace(/(\r\n){3,}/g, "\r\n\r\n");

	// Remove leading/trailing blank lines
	result = result.replace(/^[\r\n]+/, "");
	result = result.replace(/[\r\n]+$/, "");

	result = word_wrap(result, 79, 1<<30, false);

	return result;
}

// Process inline markdown formatting within a line
function md2asc_inline(text, mono, NORMAL, BOLD, ITALIC, BOLD_ITALIC,
	STRIKE, CODE_SPAN, LINK_TEXT, LINK_URL)
{
	// Code spans first (to protect their contents from further processing)
	// Replace code spans with placeholders, then restore after other processing
	var code_spans = [];
	text = text.replace(/`([^`]+)`/g, function(match, code) {
		var idx = code_spans.length;
		code_spans.push(CODE_SPAN + code + NORMAL);
		return "\x00CS" + idx + "\x00";
	});

	// Images: ![alt](url) -> [Image: alt]
	text = text.replace(/!\[([^\]]*)\]\([^)]+\)/g, "[Image: $1]");

	// Links: [text](url)
	text = text.replace(/\[([^\]]+)\]\(([^)]+)\)/g,
		LINK_TEXT + "$1" + NORMAL + " [" + LINK_URL + "$2" + NORMAL + "]");

	// Reference-style links: [text][ref] -> just show text
	text = text.replace(/\[([^\]]+)\]\[[^\]]*\]/g, "$1");

	// Autolinks: <url>
	text = text.replace(/<(https?:\/\/[^>]+)>/g, LINK_URL + "$1" + NORMAL);

	// Strip any remaining HTML tags in inline text
	text = text.replace(/<\/?[a-z][a-z0-9]*(?:\s[^>]*)?\/?>/gi, "");

	// Bold+Italic: ***text*** or ___text___
	text = text.replace(/\*{3}(.+?)\*{3}/g, BOLD_ITALIC + "$1" + NORMAL);
	text = text.replace(/_{3}(.+?)_{3}/g, BOLD_ITALIC + "$1" + NORMAL);

	// Bold: **text** or __text__
	text = text.replace(/\*{2}(.+?)\*{2}/g, BOLD + "$1" + NORMAL);
	text = text.replace(/_{2}(.+?)_{2}/g, BOLD + "$1" + NORMAL);

	// Italic: *text* or _text_
	text = text.replace(/\*([^*]+)\*/g, ITALIC + "$1" + NORMAL);
	text = text.replace(/(^|\s)_([^_]+)_(\s|$|[.,;:!?)])/g, "$1" + ITALIC + "$2" + NORMAL + "$3");

	// Strikethrough: ~~text~~
	text = text.replace(/~~([^~]+)~~/g, STRIKE + "$1" + NORMAL);

	// Restore code spans
	for(var c = 0; c < code_spans.length; c++) {
		text = text.replace("\x00CS" + c + "\x00", code_spans[c]);
	}

	// Link reference definitions: [ref]: url (suppress from output)
	text = text.replace(/^\[([^\]]+)\]:\s+\S+.*$/, "");

	return text;
}
