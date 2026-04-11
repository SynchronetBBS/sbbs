// This module displays plain text files in a "LIST"-style HTML viewer

// This module can be used as a "web handler" (automatically converting
// *.txt files on the fly), by adding the following line to
// the [JavaScript] section of your ctrl/web_handler.ini file:
// txt = txt_handler.js

var filename;
var is_web = (this.http_request != undefined);

if(is_web)
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

// Serve raw text if ?raw is in the query string
if(is_web && http_request.query.raw != undefined) {
	http_reply.header["Content-Type"] = "text/plain; charset=utf-8";
	write(lines.join("\n"));
	exit();
}

var text = lines.join("\n");
var name = file.name.replace(/^.*[\/\\]/, '');
var total = lines.length;

// Escape for safe embedding in HTML
function esc(s) {
	s = s.replace(/&/g, "&amp;");
	s = s.replace(/</g, "&lt;");
	s = s.replace(/>/g, "&gt;");
	s = s.replace(/"/g, "&quot;");
	return s;
}

writeln('<!DOCTYPE html>');
writeln('<html lang="en">');
writeln('<head>');
writeln('  <meta charset="UTF-8">');
writeln('  <meta name="viewport" content="width=device-width, initial-scale=1">');
writeln('  <title>' + esc(name) + '</title>');
writeln('  <style>');
writeln('    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }');
writeln('    html, body { height: 100%; }');
writeln('    body {');
writeln('      background: #0000AA;');
writeln('      color: #AAAAAA;');
writeln("      font-family: 'Courier New', Courier, monospace;");
writeln('      font-size: 14px;');
writeln('      line-height: 1.333;');
writeln('      overflow: hidden;');
writeln('      display: flex;');
writeln('      flex-direction: column;');
writeln('    }');
writeln('    #top-bar {');
writeln('      background: #AAAAAA;');
writeln('      color: #0000AA;');
writeln('      padding: 1px 8px;');
writeln('      display: flex;');
writeln('      justify-content: space-between;');
writeln('      align-items: center;');
writeln('      flex-shrink: 0;');
writeln('      font-weight: bold;');
writeln('      white-space: nowrap;');
writeln('      overflow: hidden;');
writeln('      gap: 8px;');
writeln('    }');
writeln('    #top-bar #brand { flex-shrink: 0; color: #00AA00; }');
writeln('    #top-bar #fn    { overflow: hidden; text-overflow: ellipsis; }');
writeln('    #top-bar #pos   { flex-shrink: 0; }');
writeln('    #content {');
writeln('      flex: 1;');
writeln('      overflow-y: scroll;');
writeln('      overflow-x: auto;');
writeln('      padding: 2px 8px;');
writeln('      white-space: pre;');
writeln('      outline: none;');
writeln('      scrollbar-width: none;');
writeln('    }');
writeln('    #content::-webkit-scrollbar { display: none; }');
writeln('    #bot-bar {');
writeln('      background: #AAAAAA;');
writeln('      color: #0000AA;');
writeln('      padding: 1px 8px;');
writeln('      display: flex;');
writeln('      justify-content: space-between;');
writeln('      align-items: center;');
writeln('      flex-shrink: 0;');
writeln('      font-weight: bold;');
writeln('      white-space: nowrap;');
writeln('      overflow: hidden;');
writeln('    }');
writeln('    #search-bar {');
writeln('      background: #AAAAAA;');
writeln('      color: #0000AA;');
writeln('      padding: 1px 8px;');
writeln('      display: none;');
writeln('      align-items: center;');
writeln('      flex-shrink: 0;');
writeln('      font-weight: bold;');
writeln('      gap: 4px;');
writeln('    }');
writeln('    #search-bar.active { display: flex; }');
writeln('    #search-input {');
writeln('      background: #0000AA;');
writeln('      color: #FFFFFF;');
writeln('      border: none;');
writeln('      outline: none;');
writeln('      font-family: inherit;');
writeln('      font-size: inherit;');
writeln('      caret-color: #FFFFFF;');
writeln('      flex: 1;');
writeln('    }');
writeln('    @media (max-width: 600px) {');
writeln('      body { font-size: 12px; }');
writeln("      #bot-bar span:first-child { display: none; }");
writeln("      #bot-bar::before { content: 'Swipe \\B7 /=Search \\B7 Q=Back'; }");
writeln('    }');
writeln('  </style>');
writeln('</head>');
writeln('<body>');
writeln('  <div id="top-bar">');
writeln('    <span id="fn">' + esc(name.toUpperCase()) + '</span>');
writeln('    <span id="brand">LIST&nbsp;v9.1</span>');
writeln('    <span id="pos">Line&nbsp;1&nbsp;/&nbsp;' + total + '</span>');
writeln('  </div>');
writeln('  <div id="content" tabindex="0">' + esc(text) + '</div>');
writeln('  <div id="bot-bar">');
writeln('    <span>PgDn/PgUp &middot; Home/End &middot; /=Search &middot; n=Next &middot; Q=Back &middot; <a href="?raw" style="color: #0000AA; text-decoration: underline;">Raw</a></span>');
writeln('    <span id="pct">0%</span>');
writeln('  </div>');
writeln('  <div id="search-bar">');
writeln('    <span>Search&nbsp;forward:&nbsp;</span>');
writeln('    <input id="search-input" type="text" autocomplete="off" spellcheck="false">');
writeln('  </div>');
writeln('');
writeln('  <script>');
writeln('  (function () {');
writeln('    var contentEl  = document.getElementById("content");');
writeln('    var posEl      = document.getElementById("pos");');
writeln('    var pctEl      = document.getElementById("pct");');
writeln('    var botBar     = document.getElementById("bot-bar");');
writeln('    var searchBar  = document.getElementById("search-bar");');
writeln('    var searchInp  = document.getElementById("search-input");');
writeln('    var totalLines = ' + total + ';');
writeln('    var lineH      = 0;');
writeln('    var lastSearch = "";');
writeln('');
writeln('    function getLineH() {');
writeln('      if (!lineH) lineH = parseFloat(getComputedStyle(contentEl).lineHeight);');
writeln('      return lineH;');
writeln('    }');
writeln('');
writeln('    function updatePos() {');
writeln('      var lh   = getLineH();');
writeln('      var line = Math.min(Math.floor(contentEl.scrollTop / lh) + 1, totalLines || 1);');
writeln('      var pct  = totalLines > 0 ? Math.round(line / totalLines * 100) : 0;');
writeln('      posEl.textContent = "Line\\u00a0" + line + "\\u00a0/\\u00a0" + totalLines;');
writeln('      pctEl.textContent = pct + "%";');
writeln('    }');
writeln('');
writeln('    function scrollLines(n) {');
writeln('      contentEl.scrollBy(0, n * getLineH());');
writeln('    }');
writeln('');
writeln('    function doSearch(q) {');
writeln('      if (!q) return;');
writeln('      lastSearch = q;');
writeln('      var text  = contentEl.textContent;');
writeln('      var lower = text.toLowerCase();');
writeln('      var lh    = getLineH();');
writeln('      var start = Math.floor(contentEl.scrollTop / lh) * lh + 1;');
writeln('      var idx   = lower.indexOf(q.toLowerCase(), start);');
writeln('      if (idx < 0) idx = lower.indexOf(q.toLowerCase(), 0);');
writeln('      if (idx < 0) return;');
writeln('      var line = text.substring(0, idx).split("\\n").length - 1;');
writeln('      contentEl.scrollTo(0, line * lh);');
writeln('    }');
writeln('');
writeln('    contentEl.addEventListener("scroll", updatePos);');
writeln('    contentEl.focus();');
writeln('');
writeln('    function openSearch() {');
writeln('      botBar.style.display = "none";');
writeln('      searchBar.classList.add("active");');
writeln('      searchInp.value = lastSearch;');
writeln('      searchInp.focus();');
writeln('      searchInp.select();');
writeln('    }');
writeln('    function closeSearch() {');
writeln('      searchBar.classList.remove("active");');
writeln('      botBar.style.display = "";');
writeln('      contentEl.focus();');
writeln('    }');
writeln('    searchInp.addEventListener("keydown", function (e) {');
writeln('      if (e.key === "Enter")  { doSearch(searchInp.value); closeSearch(); }');
writeln('      if (e.key === "Escape") { closeSearch(); }');
writeln('    });');
writeln('');
writeln('    document.addEventListener("keydown", function (e) {');
writeln('      if (searchBar.classList.contains("active")) return;');
writeln('      var page = contentEl.clientHeight;');
writeln('      switch (e.key) {');
writeln('        case "ArrowDown": case "j": e.preventDefault(); scrollLines(1);  break;');
writeln('        case "ArrowUp":   case "k": e.preventDefault(); scrollLines(-1); break;');
writeln('        case "PageDown":  case " ": e.preventDefault(); contentEl.scrollBy(0,  page); break;');
writeln('        case "PageUp":              e.preventDefault(); contentEl.scrollBy(0, -page); break;');
writeln('        case "Home":                e.preventDefault(); contentEl.scrollTo(0, 0); break;');
writeln('        case "End":                 e.preventDefault(); contentEl.scrollTo(0, contentEl.scrollHeight); break;');
writeln('        case "n": case "N":         if (lastSearch) { e.preventDefault(); doSearch(lastSearch); } break;');
writeln('        case "/":                   e.preventDefault(); openSearch(); break;');
writeln('        case "q": case "Q":');
writeln('          if (history.length > 1) history.back(); else window.close();');
writeln('          break;');
writeln('      }');
writeln('    });');
writeln('');
writeln('    var ty0 = 0;');
writeln('    contentEl.addEventListener("touchstart", function (e) { ty0 = e.touches[0].clientY; }, { passive: true });');
writeln('    contentEl.addEventListener("touchmove",  function (e) {');
writeln('      var dy = ty0 - e.touches[0].clientY;');
writeln('      contentEl.scrollBy(0, dy);');
writeln('      ty0 = e.touches[0].clientY;');
writeln('    }, { passive: true });');
writeln('  }());');
writeln('  </script>');
writeln('</body>');
writeln('</html>');
