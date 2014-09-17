function html2asc(buf, mono)
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

	if (buf==undefined)
		return buf;
	if (mono===undefined)
		mono = false;
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
	//									.replace(/\r/gi,"&#13;")
										.replace(/\n/gi,"&#10;"); 
						} 
					);

	// Condense white-space
	buf=buf.replace(/>\s*\n/g,">");					// Replace >\r\n with >
	buf=buf.replace(/\s+/g," ");					// Replace white-space with single space

	// Strip blocks
	buf=buf.replace(/<head[^<]*>.*<\/head>/gi,"");	// Strip the header

	// Links
	buf=buf.replace(/<a\s+[^<]*href\s*=\s*([^> ]*)[^>]*>([^<]*)<\/a>/gi,"$2 [$1]");

	// Visual white-space
	buf=buf.replace(/<br>/gi,"\r\n");				// Replace <br> with \r\n
	buf=buf.replace(/<p>/gi,"\r\n");				// Replace <p> with \r\n
	buf=buf.replace(/<tr[^<]*>/gi,"\r\n");			// Replace <tr> with \r\n
	buf=buf.replace(/<\/p>/gi,"\r\n");				// Replace </p> with \r\n
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
	buf=buf.replace(/<h4[^<]*>([^<]*)<\/h4>/gi,"\r\n" + HEADING4 + "-=< $1 >=-" + NORMAL + "\r\n");
	buf=buf.replace(/<h5[^<]*>([^<]*)<\/h5>/gi,"\r\n" + HEADING5 + "... $1 ..." + NORMAL + "\r\n");
	buf=buf.replace(/<h6[^<]*>([^<]*)<\/h6>/gi,"\r\n" + HEADING6 + "___ $1 ___" + NORMAL + "\r\n");

	// Lists
	buf=buf.replace(/<li[^<]*>/gi,LIST_ITEM);

	// Strip unsupported tags
	buf=buf.replace(/<[^<]*>/g,"");

	// Translate &xxx; codes to ASCII and ex-ASCII
	buf=html_decode(buf);

	buf=word_wrap(buf);

	buf+="\r\n";
	return buf;
}
