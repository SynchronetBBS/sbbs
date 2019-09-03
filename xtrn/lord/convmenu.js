require(js.exec_dir + 'convert.js', 'convert_menus');

var i;
if (!file_exists(system.temp_dir))
	mkdir(system.temp_dir);
var tmpdir = system.temp_dir + 'LORDmenus/';
if (!file_exists(tmpdir))
	mkdir(tmpdir);
var d;
var outbase = js.exec_dir + 'menus/';
var out;
var m;
var lt;
var le;
var lg;
var tt;
var tf;

for (i=0; i<argv.length; i++) {
	d = directory(tmpdir+'*');
	d.forEach(function(o) {
		file_remove(o);
	});
	if (argv[i][0] !== '/')
		argv[i] = js.exec_dir + argv[i];
	m = argv[i].match(/([^\/\\\.]+)\.[Zz][Ii][Pp]$/);
	if (m === null)
		continue;
	out = outbase + m[1] + '/';
	if (!file_exists(out))
		mkdir(out);
	if (system.exec('unzip -d \''+tmpdir + '\' -L \''+argv[i]+'\'') === 0) {
		lt = le = lg = tt = undefined;
		if (file_exists(tmpdir+'lordtxt.dat'))
			lt = tmpdir+'lordtxt.dat';
		if (file_exists(tmpdir+'lordext.dat'))
			le = tmpdir+'lordext.dat';
		if (file_exists(tmpdir+'lgametxt.dat'))
			lg = tmpdir+'lgametxt.dat';
		if (file_exists(tmpdir+'traintxt.dat'))
			tt = tmpdir+'traintxt.dat';

		// If there's a single *.dat file, use it.
		if (lt === undefined) {
			d = directory(tmpdir+'*.dat');
			if (d.length === 1) {
				tf = new File(d[0]);
				if (tf.open('r')) {
					if (tf.read(2) == '@#') {
						print('!!! Using '+d[0]+' as lordtxt.dat');
						lt = d[0];
					}
					tf.close();
				}
			}
		}

		// If there's a lordtxt*.*, use it.
		if (lt === undefined) {
			d = directory(tmpdir+'lordtxt*');
			d.forEach(function(f) {
				if (lt !== undefined)
					return;
				tf = new File(d[0]);
				if (tf.open('r')) {
					if (tf.read(2) == '@#') {
						print('!!! Using '+d[0]+' as lordtxt.dat');
						lt = d[0];
					}
					tf.close();
				}
			});
		}

		// If there's a single *txt.dat file, use it.
		if (lt === undefined) {
			d = directory(tmpdir+'*txt.dat');
			if (d.length === 1) {
				tf = new File(d[0]);
				if (tf.open('r')) {
					if (tf.read(2) == '@#') {
						print('!!! Using '+d[0]+' as lordtxt.dat');
						lt = d[0];
					}
					tf.close();
				}
			}
		}

		if (lt !== undefined || le !== undefined || lg !== undefined || tt !== undefined)
			convert_menus(lt, le, lg, tt, js.exec_dir + 'lordtxt.lrd', out+'lordtxt.lrd');
		else
			print('--== NO LORDTXT FOUND! ==--');
		if (file_exists(tmpdir+'lenemy.dat')) {
			convert_lenemy(tmpdir+'lenemy.dat', out+'lenemy.bin');
		}
		else {
			d = directory(tmpdir+'lenemy*');
			if (d.length === 1) {
				print('!!! Using '+d[0]+' as lenemy.dat');
				convert_lenemy(d[0], out+'lenemy.bin');
			}
			else {
				d = directory(tmpdir+'*enemy.dat');
				if (d.length === 1) {
					print('!!! Using '+d[0]+' as lenemy.dat');
					convert_lenemy(d[0], out+'lenemy.bin');
				}
			}
		}
		if (file_exists(tmpdir+'trainers.dat')) {
			convert_lenemy(tmpdir+'trainers.dat', out+'trainers.bin');
		}
		if (file_exists(tmpdir+'weapons.dat')) {
			convert_weap_arm(tmpdir+'weapons.dat', out+'weapons.bin');
		}
		if (file_exists(tmpdir+'armor.dat')) {
			convert_weap_arm(tmpdir+'armor.dat', out+'armor.bin');
		}
		if (file_exists(tmpdir+'armor.dat')) {
			convert_weap_arm(tmpdir+'armor.dat', out+'armor.bin');
		}
		if (file_exists(tmpdir+'start1') && file_exists(tmpdir+'start2') && file_exists(tmpdir+'start3') && file_exists(tmpdir+'start4') && file_exists(tmpdir+'start5')) {
			file_copy(tmpdir+'start1', out+'start1.lrd');
			file_copy(tmpdir+'start2', out+'start2.lrd');
			file_copy(tmpdir+'start3', out+'start3.lrd');
			file_copy(tmpdir+'start4', out+'start4.lrd');
			file_copy(tmpdir+'start5', out+'start5.lrd');
		}
		else if (file_exists(tmpdir+'bar.txt')) {
			file_copy(tmpdir+'bar.txt', out+'start1.lrd');
			file_copy(tmpdir+'bar.txt', out+'start2.lrd');
			file_copy(tmpdir+'bar.txt', out+'start3.lrd');
			file_copy(tmpdir+'bar.txt', out+'start4.lrd');
			file_copy(tmpdir+'bar.txt', out+'start5.lrd');
		}
		if (file_exists(tmpdir+'dstart'))
			file_copy(tmpdir+'dstart', out+'dstart.lrd');
		else if (file_exists(tmpdir+'darkbar.txt'))
			file_copy(tmpdir+'darkbar.txt', out+'dstart.lrd');
		if (file_exists(tmpdir+'garden.txt'))
			file_copy(tmpdir+'garden.txt', out+'gstart.lrd');
		if (file_exists(tmpdir+'goodsay.dat'))
			file_copy(tmpdir+'goodsay.dat', out+'goodsay.lrd');
		else {
			d = directory(tmpdir+'goodsay.*');
			if (d.length === 1) {
				print('Using '+d[0]+' as goodsay');
				file_copy(d[0], out+'goodsay.lrd');
			}
		}
		if (file_exists(tmpdir+'normsay.dat'))
			file_copy(tmpdir+'normsay.dat', out+'normsay.lrd');
		else {
			d = directory(tmpdir+'normsay.*');
			if (d.length === 1) {
				print('Using '+d[0]+' as normsay');
				file_copy(d[0], out+'normsay.lrd');
			}
		}
		if (file_exists(tmpdir+'badsay.dat'))
			file_copy(tmpdir+'badsay.dat', out+'badsay.lrd');
		else {
			d = directory(tmpdir+'badsay.*');
			if (d.length === 1) {
				print('Using '+d[0]+' as badsay');
				file_copy(d[0], out+'badsay.lrd');
			}
		}

		d = directory(tmpdir+'*.diz');
		d.forEach(function(f) {
			file_copy(f, out+file_getname(f));
		});
		d = directory(tmpdir+'*.doc');
		d.forEach(function(f) {
			file_copy(f, out+file_getname(f));
		});
		d = directory(tmpdir+'*read*.*');
		d.forEach(function(f) {
			file_copy(f, out+file_getname(f));
		});
		d = directory(tmpdir+'*.reg');
		d.forEach(function(f) {
			file_copy(f, out+file_getname(f));
		});
		d = directory(tmpdir+'*.1st');
		d.forEach(function(f) {
			file_copy(f, out+file_getname(f));
		});
		d = directory(tmpdir+'*.ans');
		d.forEach(function(f) {
			file_copy(f, out+file_getname(f));
		});
		if (file_exists(tmpdir+m[1]+'.txt'))
			file_copy(tmpdir+m[1]+'.txt', out+m[1]+'.txt')
		if (file_exists(tmpdir+'anote.txt'))
			file_copy(tmpdir+m[1]+'.txt', out+'anote.txt')
		// BADWORDS?!?!
	}
}
