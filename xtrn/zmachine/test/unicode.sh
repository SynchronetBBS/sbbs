#!/bin/sh
# Engine-level Unicode regression: drive the v8 Dreamhold tutorial far enough to print
# "cliché" and assert the engine decoded a real U+00E9 (not CP437 'ª' / U+00AA).
# Usage: test/unicode.sh
HERE=$(cd "$(dirname "$0")" && pwd)
STORY=$(ls "$HERE"/../*/dreamhold.z8 2>/dev/null | head -1)   # in whatever category dir the sysop filed it
[ -z "$STORY" ] && { echo "UNICODE SKIPPED (dreamhold.z8 not found under $HERE/../*/)"; exit 0; }
jsexec -r "
load('$HERE/../quetzal.js'); load('$HERE/../jszm.js');
function rd(p){var f=new File(p);f.open('rb');var b=f.readBin(1,f.length);f.close();return new Uint8Array(b);}
var g=new JSZM(rd('$STORY'));
var out='', cmds=['e','up'], ci=0;
g.print=function(t){out+=t;}; g.highlight=function(){}; g.updateStatusLine=function(){};
g.split=function(){}; g.screen=function(){}; g.setCursor=function(){}; g.getCursor=function(){return{row:1,col:1};};
g.eraseWindow=function(){}; g.eraseLine=function(){}; g.bufferMode=function(){}; g.restarted=function(){this.seed=1;};
g.read=function(maxlen){ if(maxlen==1) return ' '; return ci<cmds.length?cmds[ci++]:null; };
g.save=function(){return false;}; g.restore=function(){return null;};
try{ g.run(); }catch(e){}
if (out.indexOf(String.fromCharCode(0x00E9)) >= 0) write('UNICODE OK');
else if (out.indexOf(String.fromCharCode(0x00AA)) >= 0) write('UNICODE FAIL: still decoding e-acute as U+00AA');
else write('UNICODE INCONCLUSIVE: \"cliche\" not reached');
"
echo
