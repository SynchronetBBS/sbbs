#!/bin/sh
# v6 text-narrative parity: our engine vs dfrotz on Arthur (graphics off).
#
# Both interpreters produce the SAME story narrative; the differences are purely presentational and
# are normalized away SYMMETRICALLY (never so aggressively that a real wrong-narrative regression
# would be hidden):
#   - Line wrapping: the headless engine emits unwrapped paragraphs; dfrotz hard-wraps at its width;
#     the real door wraps at runtime. We flatten each blank-line-separated paragraph to one line.
#   - Chrome: the ">" input prompt, Arthur's own echo of the typed command (e.g. ">LOOK"), the
#     restore/quit "Please press Y or N>" prompts and their y/n echo, the v6 status line ("...St
#     Anne's Day..."), dfrotz's left margin, and dfrotz's "Using normal formatting"/"Loading"/
#     "Warning: @draw_picture ... resources are missing" banners. All stripped from BOTH sides.
# What remains is the actual game narrative (intro, Merlin speech, room descriptions, parser
# responses), which must match byte-for-byte. Skips cleanly if the (copyrighted) story or dfrotz is absent.
HERE=$(cd "$(dirname "$0")" && pwd)
STORY=$(ls "$HERE"/../*/[Aa]rthur*.z6 2>/dev/null | head -1)
DFROTZ="${DFROTZ:-/usr/games/dfrotz}"
[ -n "$STORY" ] || { echo "V6 PARITY SKIPPED (Arthur*.z6 not found under $HERE/../*/)"; exit 0; }
[ -x "$DFROTZ" ] || { echo "V6 PARITY SKIPPED (dfrotz not found: $DFROTZ)"; exit 0; }

# 8 keypresses clear Arthur's read_char intro (6 of them) and become two harmless 'go north' moves;
# then 'look' (a room description), 'quit', and 'y' (the quit confirmation read_char).
# One word-list drives BOTH: newline-fed to dfrotz; built into a JS array for our headless driver
# (jsexec -r does not reliably read piped stdin, so we inline the inputs).
WORDS="n n n n n n n n look quit y"
JSARR=$(for w in $WORDS; do printf '"%s",' "$w"; done)

norm() {
  sed 's/\r//g' \
  | grep -viE 'Using normal formatting|^Loading |^Warning:|St Anne' \
  | sed -E 's/^[[:space:]]+//' \
  | sed -E 's/Please press Y or N>//g' \
  | sed -E 's/^>[A-Za-z]+$//' \
  | sed -E 's/^>[[:space:]]*//' \
  | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//' \
  | sed -E '/^[yn]$/d' \
  | awk '/^[[:space:]]*$/{if(p){print p;p=""};next}{p=(p?p" "$0:$0)}END{if(p)print p}' \
  | sed -E 's/[[:space:]]+/ /g'
}

# Our engine, headless: capture only window 0 (the scrolling narrative); other windows are chrome.
OURS=$(jsexec -r "
load('$HERE/../quetzal.js'); load('$HERE/../jszm.js');
function rd(p){var f=new File(p);f.open('rb');var b=f.readBin(1,f.length);f.close();return new Uint8Array(b);}
var g=new JSZM(rd('$STORY')); var out=''; var win=0; var lines=[$JSARR]; var i=0;
g.print=function(t){ if(win===0) out+=t; };
g.highlight=function(){}; g.updateStatusLine=function(){}; g.screen=function(w){win=w;};
g.split=function(){}; g.setCursor=function(){}; g.getCursor=function(){return{row:1,col:1};};
g.eraseWindow=function(){}; g.eraseLine=function(){}; g.bufferMode=function(){};
g.windowChanged=function(){}; g.scrollWindow=function(){}; g.graphicsAvailable=function(){return false;};
g.setFont=function(){return 0;}; g.drawPicture=function(){}; g.erasePicture=function(){}; g.pictureInfo=function(){return null;};
g.restarted=function(){this.seed=1;}; g.save=function(){return false;}; g.restore=function(){return null;};
g.read=function(){return i<lines.length?lines[i++]:null;};
try{g.run();}catch(e){}
write(out);
" 2>/dev/null | norm)

REF=$(printf '%s\n' $WORDS | "$DFROTZ" -p -w 200 -h 999 "$STORY" 2>/dev/null | norm)

if [ -n "$OURS" ] && [ "$OURS" = "$REF" ]; then
  echo "V6 PARITY OK"
else
  echo "V6 PARITY DIFF:"
  diff <(printf '%s\n' "$REF") <(printf '%s\n' "$OURS") | head -40
  exit 1
fi
