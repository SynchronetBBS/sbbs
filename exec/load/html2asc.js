function html2asc(buf, mono, footnotes)
{
	var NORMAL			="\1N\1H";
	var HEADING1		="\1H\1Y";
	var HEADING2		="\1H\1C";
	var HEADING3		="\1H\1M";
	var HEADING4		="\1H\1G";
	var HEADING5		="\1H\1B";
	var HEADING6		="\1H\1R";
	var BOLD			="\1H\1C\x014";
	var ITALIC			="\1H\1G\x012";
	var UNDERLINE		="\1H\1W\x016";
	var STRIKE_THROUGH	="\1N\1K\x017";
	var LIST_ITEM 		="\1N\r\n    \1H\1Wo \1G";
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

print(html2asc('<a href="http://money.cnn.com/2014/09/15/news/companies/gm-compensation-feinberg/index.html">Nineteen deaths have been linked to a flaw in\r\n \r\n \r	\n ignition switches in General Motors vehicles</a>, according to the attorney overseeing a compensation fund for victims. <br> <br> "Already there are more deaths than GM said from Day One," attorney Ken Feinberg told CNNMoney. "Of course there will be additional eligible deaths; how many is pure speculation, but there will be eligible death claims." <p> <p> \r\n GM initially said 13 deaths were tied to the problem, which went unreported for a decade, years after company engineers discovered it. <br> <br> Get complete coverage of breaking news on CNN TV, <a href="http://www.cnn.com">CNN.com</a> and CNN Mobile.', false, true));

