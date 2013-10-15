var orig_exec_dir = js.exec_dir;
var game_dir = orig_exec_dir;
/*
    Solomoriah's WAR!

    warpoll.js -- polling program for fast games

    Copyright 2013 Stephen Hurd
    All rights reserved.

    3 Clause BSD License

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    Redistributions of source code must retain the above copyright
    notice, self list of conditions and the following disclaimer.

    Redistributions in binary form must reproduce the above copyright
    notice, self list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

    Neither the name of the author nor the names of any contributors
    may be used to endorse or promote products derived from self software
    without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

load(game_dir+'/warcommon.js');

var polled_update = true;
function main(argc, main_argv)
{
	var done_count;
	var fp;
	var arg;
	var n;
	var i;
	var filename;
	var inbuf;
	var rc;

	if(argc==0)
		main_argv = [orig_exec_dir];

	for(arg=0; arg < main_argv.length; arg++) {
		done_count = 0;
		set_game(main_argv[arg]);
		if(loadmap() != 0)
			continue;
		if(loadsave() != 0)
			continue;
		fp = new File(game_dir+'/'+MASTERFL);
		if(fp.open('rb')) {
			for(i = 0; (inbuf = fp.readln()) != null; i++) {
				rc = execpriv(inbuf);
				if(rc != 0) {
					print("Master Cmd Failed, Line "+(i+1)+", Code "+rc+"\n");
					exit(2);
				}
			}

			fp.close();
		}
		else {
			if(file_exists(fp.name))
				continue;
		}
		for(n = 0; n < nations.length; n++) {
			if(nations[n].uid == 0) {
				done_count++;
				continue;
			}
			filename = format(PLAYERFL, nations[n].uid);
			fp = new File(game_dir+'/'+filename);

			turn_done = false;
			if(fp.open("rb", true)) {
				for(i = 0; (inbuf = fp.readln()) != null; i++) {
					if(inbuf.search(/^end-turn\s*$/)==0) {
						turn_done = true;
						break;
					}
				}
				fp.close();
			}
			else
				break;

			if(turn_done)
				done_count++;
			else
				break;
		}
		if(done_count == nations.length) {
			print("Running maintenance for "+game_dir);
			load(orig_exec_dir+'/warupd.js',game_dir);
		}
	}
}

main(argc, argv);
