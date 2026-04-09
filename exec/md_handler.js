// This module converts Markdown (.md) files to HTML

// This module can be used as a "web handler" (automatically converting
// *.md files on the fly), by adding the following line to
// the [JavaScript] section of your ctrl/web_handler.ini file:
// md = md_handler.js

var filename;

if(this.http_request != undefined)	/* Requested through web-server */
	filename = http_request.real_path;
else
	filename = argv[0];

var file = new File(filename);
if(!file.open("r", true)) {
	writeln("!ERROR " + file.error + " opening " + filename);
	exit();
}
var lines = file.readAll(256 * 1024);
file.close();

var text = lines.join("\n");

// ---- Minimal Markdown-to-HTML converter ----

function esc(s) {
	s = s.replace(/&/g, "&amp;");
	s = s.replace(/</g, "&lt;");
	s = s.replace(/>/g, "&gt;");
	s = s.replace(/"/g, "&quot;");
	return s;
}

function inline(s) {
	// Images: ![alt](url)
	s = s.replace(/!\[([^\]]*)\]\(([^)]+)\)/g, '<img src="$2" alt="$1">');
	// Links: [text](url)
	s = s.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2">$1</a>');
	// Bold+italic: ***text*** or ___text___
	s = s.replace(/\*\*\*(.+?)\*\*\*/g, '<strong><em>$1</em></strong>');
	s = s.replace(/___(.+?)___/g, '<strong><em>$1</em></strong>');
	// Bold: **text** or __text__
	s = s.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
	s = s.replace(/__(.+?)__/g, '<strong>$1</strong>');
	// Italic: *text* or _text_
	s = s.replace(/\*(.+?)\*/g, '<em>$1</em>');
	s = s.replace(/\b_(.+?)_\b/g, '<em>$1</em>');
	// Inline code: `code`
	s = s.replace(/`([^`]+)`/g, '<code>$1</code>');
	return s;
}

function convert(text) {
	var out = [];
	var lines = text.split("\n");
	var i = 0;
	var len = lines.length;

	while(i < len) {
		var line = lines[i];

		// Fenced code block: ``` or ~~~
		if(/^(`{3,}|~{3,})/.test(line)) {
			var fence = RegExp.$1;
			var closer = new RegExp("^" + fence.charAt(0) + "{" + fence.length + ",}\\s*$");
			var code = [];
			i++;
			while(i < len && !closer.test(lines[i])) {
				code.push(esc(lines[i]));
				i++;
			}
			if(i < len) i++; // skip closing fence
			out.push("<pre><code>" + code.join("\n") + "</code></pre>");
			continue;
		}

		// Blank line
		if(/^\s*$/.test(line)) {
			i++;
			continue;
		}

		// ATX headings: # through ######
		var m = line.match(/^(#{1,6})\s+(.*?)(?:\s+#+)?\s*$/);
		if(m) {
			var level = m[1].length;
			out.push("<h" + level + ">" + inline(esc(m[2])) + "</h" + level + ">");
			i++;
			continue;
		}

		// Horizontal rule: ---, ***, ___
		if(/^(\*{3,}|-{3,}|_{3,})\s*$/.test(line)) {
			out.push("<hr>");
			i++;
			continue;
		}

		// Table: line starts with | and next line is separator |---|
		if(/^\|/.test(line) && i + 1 < len && /^\|[\s:]*-[-\s:|]*\|/.test(lines[i + 1])) {
			// Parse header row
			var hcells = line.replace(/^\|/, "").replace(/\|$/, "").split("|");
			// Parse alignment from separator row
			var sepline = lines[i + 1];
			var seps = sepline.replace(/^\|/, "").replace(/\|$/, "").split("|");
			var aligns = [];
			for(var s = 0; s < seps.length; s++) {
				var sep = seps[s].replace(/^\s+/, "").replace(/\s+$/, "");
				if(/^:-+:$/.test(sep))
					aligns.push("center");
				else if(/^-+:$/.test(sep))
					aligns.push("right");
				else
					aligns.push("");
			}
			i += 2; // skip header and separator
			// Collect body rows
			var rows = [];
			while(i < len && /^\|/.test(lines[i])) {
				rows.push(lines[i]);
				i++;
			}
			out.push("<table>");
			out.push("<thead><tr>");
			for(var h = 0; h < hcells.length; h++) {
				var cell = hcells[h].replace(/^\s+/, "").replace(/\s+$/, "");
				var align = (h < aligns.length && aligns[h]) ? ' style="text-align: ' + aligns[h] + '"' : "";
				out.push("<th" + align + ">" + inline(esc(cell)) + "</th>");
			}
			out.push("</tr></thead>");
			out.push("<tbody>");
			for(var r = 0; r < rows.length; r++) {
				var rcells = rows[r].replace(/^\|/, "").replace(/\|$/, "").split("|");
				out.push("<tr>");
				for(var c = 0; c < rcells.length; c++) {
					var cell = rcells[c].replace(/^\s+/, "").replace(/\s+$/, "");
					var align = (c < aligns.length && aligns[c]) ? ' style="text-align: ' + aligns[c] + '"' : "";
					out.push("<td" + align + ">" + inline(esc(cell)) + "</td>");
				}
				out.push("</tr>");
			}
			out.push("</tbody>");
			out.push("</table>");
			continue;
		}

		// Blockquote
		if(/^>\s?/.test(line)) {
			var bq = [];
			while(i < len && /^>\s?/.test(lines[i])) {
				bq.push(lines[i].replace(/^>\s?/, ""));
				i++;
			}
			out.push("<blockquote>" + convert(bq.join("\n")) + "</blockquote>");
			continue;
		}

		// Unordered list: *, -, +
		if(/^[\s]*[*+-]\s+/.test(line)) {
			var items = [];
			var buf = [];
			while(i < len && /^[\s]*[*+-]\s+/.test(lines[i])) {
				buf.push(lines[i].replace(/^[\s]*[*+-]\s+/, ""));
				i++;
				// Continuation lines (indented)
				while(i < len && /^\s+\S/.test(lines[i]) && !/^[\s]*[*+-]\s+/.test(lines[i])) {
					buf[buf.length - 1] += " " + lines[i].replace(/^\s+/, "");
					i++;
				}
			}
			out.push("<ul>");
			for(var j = 0; j < buf.length; j++)
				out.push("<li>" + inline(esc(buf[j])) + "</li>");
			out.push("</ul>");
			continue;
		}

		// Ordered list: 1. 2. etc.
		if(/^\s*\d+\.\s+/.test(line)) {
			var items = [];
			while(i < len && /^\s*\d+\.\s+/.test(lines[i])) {
				items.push(lines[i].replace(/^\s*\d+\.\s+/, ""));
				i++;
				while(i < len && /^\s+\S/.test(lines[i]) && !/^\s*\d+\.\s+/.test(lines[i])) {
					items[items.length - 1] += " " + lines[i].replace(/^\s+/, "");
					i++;
				}
			}
			out.push("<ol>");
			for(var j = 0; j < items.length; j++)
				out.push("<li>" + inline(esc(items[j])) + "</li>");
			out.push("</ol>");
			continue;
		}

		// Setext heading: underlined with === or ---
		if(i + 1 < len && /^={2,}\s*$/.test(lines[i + 1])) {
			out.push("<h1>" + inline(esc(line)) + "</h1>");
			i += 2;
			continue;
		}
		if(i + 1 < len && /^-{2,}\s*$/.test(lines[i + 1])) {
			out.push("<h2>" + inline(esc(line)) + "</h2>");
			i += 2;
			continue;
		}

		// Paragraph: collect consecutive non-blank, non-special lines
		var para = [];
		while(i < len && !/^\s*$/.test(lines[i])
			&& !/^(#{1,6})\s+/.test(lines[i])
			&& !/^(`{3,}|~{3,})/.test(lines[i])
			&& !/^>\s?/.test(lines[i])
			&& !/^(\*{3,}|-{3,}|_{3,})\s*$/.test(lines[i])
			&& !/^[\s]*[*+-]\s+/.test(lines[i])
			&& !/^\s*\d+\.\s+/.test(lines[i])
			&& !/^\|/.test(lines[i])) {
			para.push(lines[i]);
			// Check for setext underline on next line
			if(i + 1 < len && (/^={2,}\s*$/.test(lines[i + 1]) || /^-{2,}\s*$/.test(lines[i + 1])))
				break;
			i++;
		}
		if(para.length > 0)
			out.push("<p>" + inline(esc(para.join("\n"))) + "</p>");
	}

	return out.join("\n");
}

var html = convert(text);
var title = file.name.replace(/^.*[\/\\]/, '');

writeln('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">');
writeln("<html>");
writeln("<head>");
writeln("<meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>");
writeln("<title>" + esc(title) + "</title>");
writeln("<style type='text/css'>");
writeln("body { font-family: Arial, Helvetica, sans-serif; max-width: 50em; margin: 1em auto; padding: 0 1em; }");
writeln("pre { background-color: #f4f4f4; padding: 1em; overflow-x: auto; }");
writeln("code { background-color: #f4f4f4; padding: 0.2em 0.4em; }");
writeln("pre code { padding: 0; background: none; }");
writeln("blockquote { border-left: 4px solid #ccc; margin-left: 0; padding-left: 1em; color: #555; }");
writeln("img { max-width: 100%; }");
writeln("table { border-collapse: collapse; width: 100%; margin: 1em 0; }");
writeln("th, td { border: 1px solid #ccc; padding: 0.4em 0.8em; }");
writeln("th { background-color: #f4f4f4; }");
writeln("tr:nth-child(even) { background-color: #fafafa; }");
writeln("</style>");
writeln("</head>");
writeln("<body>");
write(html);
writeln("</body>");
writeln("</html>");
