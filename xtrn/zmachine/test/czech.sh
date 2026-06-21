#!/bin/sh
# Run a czech.zN through our engine via a tiny jsexec driver and print its output.
# Usage: test/czech.sh <version-digit>   e.g.  test/czech.sh 5
HERE=$(cd "$(dirname "$0")" && pwd); V="${1:-5}"
jsexec -r "
load('$HERE/../quetzal.js'); load('$HERE/../jszm.js');
function rd(p){var f=new File(p);f.open('rb');var b=f.readBin(1,f.length);f.close();return new Uint8Array(b);}
var g=new JSZM(rd('$HERE/stories/czech.z$V'));
var out='';
g.print=function(t){out+=t;}; g.highlight=function(){}; g.updateStatusLine=function(){};
g.restarted=function(){this.seed=1;};
g.read=function(){return null;};   // czech runs autonomously; null ends input
try { g.run(); } catch(e){ out += '\n[engine error: '+e+']'; }
write(out);
"
