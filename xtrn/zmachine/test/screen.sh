#!/bin/sh
# Transcript parity for a v5 upper-window game on a scripted run: our engine vs dfrotz.
# Proves the v4+ screen opcodes (split/set_window/set_cursor/...) drive the correct
# LOWER-WINDOW narrative -- the cursor positioning and the upper window (status) are
# not part of a flat transcript, so we compare only the lower-window text the screen
# opcodes must never corrupt.
# Usage: test/screen.sh [story]   default: games/Advent.z5
#
# Why the normalization below (the soft spot): dfrotz and our headless engine present
# the SAME game narrative with different chrome around it. We normalize BOTH sides
# identically until only narrative remains -- never so aggressively that real text
# (room names, "Taken.", descriptions) would be filtered, so a wrong-narrative engine
# bug would still DIFF. The filters, and the divergence each one closes:
#
#   1. Upper-window (status) text. Advent splits a window and prints its status bar
#      (room name + "Score:" + "Moves:") into the UPPER window via set_window 1.
#      dfrotz renders that on its own interleaved line ("> Room ...  Score: N Moves: M");
#      our headless g.print would otherwise collect it inline (set_cursor is a no-op
#      here, so it would glue onto the lower-window narrative). We close this on OUR
#      side the same way the door's frame renderer does -- track the current window in
#      g.screen() and DROP upper-window (win==1) prints -- and on the REF side by
#      stripping the "Score:"/"Moves:" status lines dfrotz emits. (Narrative never
#      contains "Score:"/"Moves:" in this script, so nothing real is lost.)
#   2. The ">" input prompt. dfrotz echoes "> " before the (upper-window) status redraw;
#      our game prints just ">" in the lower window. After (1) strips the status text,
#      a leading "> "/">" prompt may remain glued to narrative on either side, so we
#      strip a leading "> ?" prompt from BOTH.
#   3. dfrotz banner lines ("Using normal formatting." / "Loading <file>.") -- ref only,
#      but the filter is applied to both sides (ours never emits them).
#   4. Blank lines -- the two interpreters differ in where window setup leaves blanks;
#      collapse them on both sides.
#
# A scripted command (the typed line itself) is never echoed by our engine and dfrotz's
# echo lives on the status-redraw line we strip, so command echo needs no extra filter.
HERE=$(cd "$(dirname "$0")" && pwd)
STORY="${1:-$(ls "$HERE"/../*/Advent.z5 2>/dev/null | head -1)}"   # in whatever category dir it's filed
DFROTZ="${DFROTZ:-/usr/games/dfrotz}"
[ -f "$STORY" ] || { echo "SCREEN PARITY SKIPPED (Advent.z5 not found under $HERE/../*/)"; exit 0; }
[ -x "$DFROTZ" ] || { echo "SCREEN PARITY SKIPPED (dfrotz not found: $DFROTZ)"; exit 0; }

# Scripted input, one command per line. Kept in a temp file so the engine driver can
# read it with File.readln() (an inline JS string can't carry embedded newlines).
CMDS=$(mktemp)
printf '%s\n' 'in' 'take lamp' 'turn on lamp' 'look' 'quit' 'y' > "$CMDS"

# Shared normalization: strip CR, strip a leading "> " prompt, drop the status-line
# (Score:/Moves:) and dfrotz banner rows, collapse blank lines. Applied identically to
# both transcripts so only game narrative is compared.
normalize() {
  sed 's/\r//g' \
    | sed 's/^> \{0,1\}//' \
    | grep -viE 'Score:|Moves:|^Using normal formatting|^Loading ' \
    | grep -vE '^[[:space:]]*$'
}

# Our engine (headless). g.screen() tracks the active window; g.print() keeps only the
# lower window (win==0) -- exactly what the door's renderer routes to the output frame.
OURS=$(jsexec -r "
load('$HERE/../quetzal.js'); load('$HERE/../jszm.js');
function rd(p){var f=new File(p);f.open('rb');var b=f.readBin(1,f.length);f.close();return new Uint8Array(b);}
var cf=new File('$CMDS'); cf.open('r'); var lines=[]; var ln; while((ln=cf.readln())!==null) lines.push(ln); cf.close();
var g=new JSZM(rd('$STORY')); var out=''; var i=0; var win=0;
g.print=function(t){ if(win==0) out+=t; };
g.highlight=function(){}; g.updateStatusLine=function(){};
g.split=function(){}; g.screen=function(w){ win=w; };
g.setCursor=function(){}; g.getCursor=function(){return{row:1,col:1};};
g.eraseWindow=function(){}; g.eraseLine=function(){}; g.bufferMode=function(){};
g.restarted=function(){this.seed=1;}; g.read=function(){return i<lines.length?lines[i++]:null;};
g.save=function(){return false;}; g.restore=function(){return null;};
try{g.run();}catch(e){}
write(out);
" 2>/dev/null | normalize)

# dfrotz reference (-p plain; tall screen -h 999 disables the ***MORE*** pager so it
# can't swallow scripted input; -w 200 keeps the status line on one row).
REF=$(printf '%s\n' 'in' 'take lamp' 'turn on lamp' 'look' 'quit' 'y' \
  | "$DFROTZ" -p -w 200 -h 999 "$STORY" 2>/dev/null | normalize)

rm -f "$CMDS"

if [ "$OURS" = "$REF" ]; then
  echo "SCREEN PARITY OK"
else
  echo "SCREEN PARITY DIFF:"
  diff <(printf '%s\n' "$REF") <(printf '%s\n' "$OURS") | head -40
  exit 1
fi
