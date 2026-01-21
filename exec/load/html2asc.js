function html2asc(buf, mono, footnotes)
{
	var NORMAL			="\x01N";
	var HEADING1		="\x01H\x01Y";
	var HEADING2		="\x01H\x01C";
	var HEADING3		="\x01H\x01M";
	var HEADING4		="\x01H\x01G";
	var HEADING5		="\x01H\x01B";
	var HEADING6		="\x01H\x01R";
	var BOLD			="\x01N\x01H";
	var ITALIC			="\x01N\x01C";
	var UNDERLINE		="\x01N\x01B";
	var STRIKE_THROUGH	="\x01N\x01K\x017";
	var LIST_ITEM 		="\x01N\r\n    \x01H\x01Wo \x01G";
	var links			=[];
	var i;

	if (buf==undefined)
		return buf;
	if (mono===undefined)
		mono = false;
	if (footnotes===undefined)
		footnotes = false;
	if(mono) {
		NORMAL			=""
		HEADING1		=""
		HEADING2		=""
		HEADING3		=""
		HEADING4		=""
		HEADING5		=""	
		HEADING6		=""
		BOLD			=""
		ITALIC			=""
		UNDERLINE		=""
		STRIKE_THROUGH	=""
		LIST_ITEM 		="\r\n    o "
	}

	// Global search and replaces

	// Tag aliases
	buf=buf.replace(/<strong>/gi,"<b>");
	buf=buf.replace(/<\/strong>/gi,"</b>");
	buf=buf.replace(/<strike>/gi,"<s>");
	buf=buf.replace(/<\/strike>/gi,"</s>");

	// Pre block (defeat white-space condensation)
	buf=buf.replace(/<pre>([^<]*)<\/pre>/gi
						,function (str, text) { 
							return  text.replace(/ /gi,"&#32;")
										.replace(/\n/gi,"&#10;"); 
						} 
					);

	// Condense white-space
	buf=buf.replace(/[ \t\r]*\n[ \t\r]*/g," ");		// Replace <WS>\n<WS> with single space
	buf=buf.replace(/[ \t\r]+/g," ");				// Replace white-space with single space

	// Strip blocks
	buf=buf.replace(/<head[^<]*>.*<\/head>/gi,"");	// Strip the header

	// Links
	if (footnotes) {
		// Proper anchor
		buf=buf.replace(/<a\s+[^<]*href\s*=\s*"([^"]*)"[^>]*>(.*?)<\/a>/gi
			,function(str, link, text) {
				links.push(link);
				return text+"["+links.length+"]";
			}
		);
		// Broken anchor (no quotes)
		buf=buf.replace(/<a\s+[^<]*href\s*=\s*([^> ]*)[^>]*>(.*?)<\/a>/gi
			,function(str, link, text) {
				links.push(link);
				return text+"["+links.length+"]";
			}
		);
	}
	else {
		// Proper anchor
		buf=buf.replace(/<a\s+[^<]*href\s*=\s*"([^"]*)"[^>]*>(.*?)<\/a>/gi,"$2 [$1]");
		// Broken anchor (no quotes)
		buf=buf.replace(/<a\s+[^<]*href\s*=\s*([^> ]*)[^>]*>(.*?)<\/a>/gi,"$2 [$1]");
	}

	// Visual white-space
	buf=buf.replace(/<br\s*\/?> */gi,"\r\n");		// Replace <br> with \r\n
	buf=buf.replace(/<p\s*\/?> */gi,"\r\n\r\n");	// Replace <p> with \r\n\r\n
	buf=buf.replace(/<tr[^<]*> */gi,"\r\n");		// Replace <tr> with \r\n
	buf=buf.replace(/<\/p> */gi,"\r\n\r\n");		// Replace </p> with \r\n
	buf=buf.replace(/<\/ul>/gi,"\r\n\r\n");			// Replace </ul>
	buf=buf.replace(/<\/ol>/gi,"\r\n");				// Replace </ol>
	buf=buf.replace(/<\/caption>/gi,"\r\n");		// Replace </caption>
	buf=buf.replace(/<\/table>/gi,"\r\n");			// Replace </table>
	buf=buf.replace(/^[\r\n]+/,"");					// Remove leading line breaks
	buf=buf.replace(/[\r\n]+$/,"");					// Remove trailing line breaks

	buf=buf.replace(/<td[^<]*>/gi," ");
	buf=buf.replace(/<th[^<]*>/gi," ");

	// Text attributes
	buf=buf.replace(/<b>([^<]*)<\/b>/gi,BOLD				+ "$1" + NORMAL);
	buf=buf.replace(/<i>([^<]*)<\/i>/gi,ITALIC				+ "$1" + NORMAL);
	buf=buf.replace(/<u>([^<]*)<\/u>/gi,UNDERLINE			+ "$1" + NORMAL);
	buf=buf.replace(/<s>([^<]*)<\/s>/gi,STRIKE_THROUGH		+ "$1" + NORMAL);

	// Headings
	buf=buf.replace(/<h1[^<]*>([^<]*)<\/h1>/gi,"\r\n" + HEADING1 + "*** $1 ***" + NORMAL + "\r\n\r\n");
	buf=buf.replace(/<h2[^<]*>([^<]*)<\/h2>/gi,"\r\n" + HEADING2 + "%%% $1 %%%" + NORMAL + "\r\n");
	buf=buf.replace(/<h3[^<]*>([^<]*)<\/h3>/gi,"\r\n" + HEADING3 + "--- $1 ---" + NORMAL + "\r\n");
	buf=buf.replace(/<h4[^<]*>([^<]*)<\/h4>/gi,"\r\n" + HEADING4 + "-=&lt; $1 &gt;=-" + NORMAL + "\r\n");
	buf=buf.replace(/<h5[^<]*>([^<]*)<\/h5>/gi,"\r\n" + HEADING5 + "... $1 ..." + NORMAL + "\r\n");
	buf=buf.replace(/<h6[^<]*>([^<]*)<\/h6>/gi,"\r\n" + HEADING6 + "___ $1 ___" + NORMAL + "\r\n");

	// Lists
	buf=buf.replace(/<li[^<]*>/gi,LIST_ITEM);

	// Collapse paragraph breaks
	buf=buf.replace(/\r\n\r\n[\r\n]+/g, "\r\n\r\n");

	// Strip unsupported tags
	buf=buf.replace(/<[^<]*>/g,"");

	// Translate &xxx; codes to ASCII and ex-ASCII
	buf=html_decode(buf);

	buf=word_wrap(buf, 79, 1<<30, false);

	if (footnotes) {
		if (links.length > 0) {
			buf += "\r\n\r\n";
			for (i=0; i<links.length; i++) {
				buf += "["+(i+1)+"] "+links[i]+"\r\n";
			}
		}
	}

	return buf;
}
