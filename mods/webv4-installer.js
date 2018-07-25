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

function get_service_ini(section) {
    const f = new File(system.ctrl_dir + 'services.ini');
    if (!f.open('r')) return false;
    const ini = f.iniGetObject(section);
    f.close();
    return ini;
}

function set_service_ini(section, obj) {
    const f = new File(system.ctrl_dir + 'services.ini');
    if (!f.open('r+')) return false;
    if (!f.iniSetObject(section, obj)) return false;
    f.close();
    return true;
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

const url_suffix = argc > 0 ? argv[0] : master;
const zip_url = 'https://codeload.github.com/echicken/synchronet-web-v4/zip/' + url_suffix;
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

var wss = get_service_ini('WS');
if (!wss) {
    var wss = {
        Port : 1123,
        Options : 'NO_HOST_LOOKUP',
        Command : 'websocketservice.js'
    };
}

var wsss = get_service_ini('WSS');
if (!wsss) {
    var wsss = {
        Port : 11235,
        Options : 'NO_HOST_LOOKUP|TLS',
        Command : 'websocketservice.js'
    };
}

write('\r\n---\r\n\r\n');
writeln('ecwebv4 installer/updater');
writeln('https://github.com/echicken/synchronet-web-v4');

if (system.version_num < 31700) {
    writeln('Synchronet versions earlier than 3.17a are not supported. Exiting.');
    exit();
}

write('\r\nIt is strongly recommended that you back up your BBS before proceeding.\r\n');
write('\r\nIf this is a new intallation, you must also shut down your BBS now.\r\n\r\n');

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

write('\r\n---\r\n\r\n');
write('Configuration - press enter to accept default/current value.\r\n\r\n');
modopts_web.guest = get_setting('Guest user alias', modopts_web.guest);
if (!system.matchuser(modopts_web.guest)) {
    writeln('Guest user does not exist. Exiting.');
    exit();
}
modopts_web.user_registration = confirm_setting('Allow new user registration via the web', modopts_web.user_registration);
modopts_web.ftelnet = confirm_setting('Enable fTelnet', modopts_web.ftelnet);
if (modopts_web.ftelnet) {
    modopts_web.ftelnet_splash = get_setting('Path to ftelnet background .ans', modopts_web.ftelnet_splash);
    write('\r\nUse of fTelnet requires a WebSocket proxy service.\r\n');
    writeln('A websocket proxy server routes traffic between a browser-based');
    writeln('application and some other arbitrary server.  Here you will configure');
    writeln('the ports that your WebSocket and WebSocket Secure proxy services will');
    writeln('listen on. Be sure to open these ports in your firewall.');
    write('\r\n');
    wss.Port = get_setting('WebSocket service port for HTTP clients', wss.Port);
    wsss.Port = get_setting('WebSocket secure service port for HTTPS clients', wsss.Port);
    writeln('Updating services.ini ...');
    if (!file_backup(system.ctrl_dir + 'services.ini')) {
        writeln('Failed to back up services.ini.  Exiting.');
        exit();
    }
    if(!set_service_ini('WS', wss)) writeln('Failed to configure WS service.');
    if (!set_service_ini('WSS', wsss)) writeln('Failed to configure WSS service.');
}

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
    writeln('If you shut down your BBS, you can restart it now.');
    writeln('For additional configuration and customization steps,');
    writeln('visit https://github.com/echicken/synchronet-web-v4');
    write('\r\n\r\n---\r\n\r\n');
}
