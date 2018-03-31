load('http.js');

function download(url, target) {
	const http = new HTTPRequest();
    try {
	    const zip = http.Get(url);
    } catch (err) {
        log(err);
        return false;
    }
	const f = new File(target);
	if (!f.open('wb')) return false;
	if (!f.write(zip)) return false;
	f.close();
    return true;
}

function extract(file, target) {
	if (!file_isdir(install_dir)) {
        if (!mkdir(install_dir)) return false;
    }
	return system.exec(system.exec_dir + 'unzip -uqo ' + file + ' -d ' + target) == 0;
}

function update_sbbs_ini(root_directory, error_directory) {
	if (!file_backup(system.ctrl_dir + 'sbbs.ini')) return false;
	const f = new File(system.ctrl_dir + 'sbbs.ini');
	if (!f.open('r+')) return false;
	if (!f.iniSetValue('Web', 'RootDirectory', root_directory)) return false;
	if (!f.iniSetValue('Web', 'ErrorDirectory', error_directory)) return false;
	f.close();
    return true;
}

function update_modopts_ini(modopts) {
	if (!file_backup(system.ctrl_dir + 'modopts.ini')) return false;
	const f = new File(system.ctrl_dir + 'modopts.ini');
	if (!f.open('r+')) return false;
	if (!f.iniSetObject('web', modopts)) return false;
	f.close();
    return true;
}

function get_modopts_ini() {
    const f = new File(system.ctrl_dir + 'modopts.ini');
    if (!f.open('r')) return false;
    const ini = f.iniGetObject('web');
    f.close();
    return ini;
}

function get_setting(text, value) {
    const i = prompt(text + ' [' + value + ']');
    return (i == '' ? value : i);
}

function confirm_setting(text, value) {
    if (!value) {
        return !deny(text + ' [' + value + ']');
    } else {
        return confirm(text + ' [' + value + ']');
    }
}

function get_settings(modopts) {
    write('\r\n---\r\n\r\n');
    write('Configuration - press enter to accept default/current value.\r\n\r\n');
    modopts.guest = get_setting('Guest user alias', modopts.guest);
    modopts.ftelnet = confirm_setting('Enable fTelnet', modopts.ftelnet);
    if (modopts.ftelnet) modopts.ftelnet_splash = get_setting('Path to ftelnet background .ans', modopts.ftelnet_splash);
    modopts.user_registration = confirm_setting('Allow new user registration via the web', modopts.user_registration);
    write('\r\n\r\n---\r\n\r\n');
    return modopts;
}

function copy_dir_contents(src, dest, overwrite) {
    src = backslash(fullpath(src));
    dest = backslash(fullpath(dest));
    const delim = src.substr(-1);
    if (!file_isdir(dest)) mkdir(dest);
    directory(src + '*').forEach(
    	function (e) {
    		e = fullpath(e);
    		if (file_isdir(e)) {
    			const path = e.split(delim);
    			var df = dest + path[path.length - 2];
    			if (!file_isdir(df)) mkdir(df);
    			copy_dir_contents(e, df, overwrite);
    		} else {
    			var df = dest + file_getname(e);
    			if (!file_exists(df) || overwrite) {
					file_copy(e, dest + file_getname(e));
    			}
    		}
    	}
    );
}

// yikes
function remove_dir(dir) {
    dir = backslash(fullpath(dir));
    directory(dir + '*').forEach(
    	function (e) {
    		if (file_isdir(e)) {
    			remove_dir(e);
    		} else {
	    		file_remove(e);
	    	}
    	}
    );
    rmdir(dir);
}

const zip_url = 'https://codeload.github.com/echicken/synchronet-web-v4/zip/master';
const download_target = system.temp_dir + 'webv4.zip';
const extract_dir = fullpath(system.temp_dir);
const temp_dir = fullpath(extract_dir + '/synchronet-web-v4-master');
const install_dir = fullpath(system.exec_dir + '../webv4');
const root_directory = fullpath(install_dir + '/root');
const error_directory = fullpath(root_directory + '/errors');

var modopts_web = get_modopts_ini();
if (!modopts_web) {
    modopts_web = {
    	guest : 'Guest',
    	timeout : 43200,
    	inactivity : 900,
    	user_registration : true,
    	minimum_password_length : 6,
    	maximum_telegram_length : 800,
    	web_directory : install_dir,
        ftelnet : true,
    	ftelnet_splash : '../text/synch.ans',
    	keyboard_navigation : false,
    	vote_functions : true,
    	refresh_interval : 60000,
    	xtrn_blacklist : 'scfg,oneliner',
    	layout_sidebar_off : false,
    	layout_sidebar_left : false,
    	layout_full_width : false,
    	forum_extended_ascii : false,
    	max_messages : 0
    };
}

write('\r\n---\r\n\r\n');
writeln('ecwebv4 installer/updater');
writeln('https://github.com/echicken/synchronet-web-v4');
write('\r\nIt is strongly recommended that you back up your BBS before proceeding.\r\n\r\n');
if (deny('Proceed with installation/update')) {
    writeln('Install/update aborted.  Exiting.');
    exit();
}
write('\r\n\r\n---\r\n\r\n');

writeln('Downloading ' + zip_url);
if (!download(zip_url, download_target)) {
    writeln('Download of ' + zip_url + ' failed. Exiting.');
    exit();
}

writeln('Extracting ' + download_target);
if (!extract(download_target, extract_dir)) {
    writeln('Extraction of ' + download_target + ' failed. Exiting.');
    exit();
}

writeln('Copying files ...');
copy_dir_contents(temp_dir + '/mods', system.mods_dir, true);
copy_dir_contents(temp_dir + '/text', system.text_dir, true);
copy_dir_contents(temp_dir + '/web', install_dir, true);
copy_dir_contents(temp_dir + '/web/pages/.examples', install_dir + '/pages', false);
copy_dir_contents(temp_dir + '/web/pages/.examples', install_dir + '/pages/.examples', true);
copy_dir_contents(temp_dir + '/web/sidebar/.examples', install_dir + '/sidebar', false);
copy_dir_contents(temp_dir + '/web/sidebar/.examples', install_dir + '/sidebar/.examples', true);

writeln('Cleaning up ...');
remove_dir(temp_dir + '/web/pages/.examples');
remove_dir(temp_dir + '/web/sidebar/.examples');
remove_dir(temp_dir);
file_remove(download_target);

modopts_web = get_settings(modopts_web);

writeln('Updating modopts.ini ...');
if (!update_modopts_ini(modopts_web)) {
    writeln('Failed to update modopts.ini. Exiting.');
    exit();
}

writeln('Updating sbbs.ini ...');
if (!update_sbbs_ini(root_directory, error_directory)) {
    writeln('Failed to update sbbs.ini. Exiting.');
    exit();
} else {
    write('\r\n---\r\n\r\n');
    writeln('Install/update complete.');
    writeln('Changes will take effect once your BBS has been restarted.');
    writeln('For additional configuration and customization steps,');
    writeln('visit https://github.com/echicken/synchronet-web-v4');
    write('\r\n\r\n---\r\n\r\n');
}
