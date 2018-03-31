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

function get_settings(modopts) {
    writeln('---');
    modopts.guest = get_setting('Guest user alias', modopts.guest);
    modopts.ftelnet_splash = get_setting('Path to ftelnet background .ans', modopts.ftelnet_splash);
    return modopts;
}

const zip_url = 'https://codeload.github.com/echicken/synchronet-web-v4/zip/master';
const download_target = system.temp_dir + 'webv4.zip';
const extract_dir = fullpath(system.exec_dir + '../web');
const install_dir = fullpath(extract_dir + '/synchronet-web-v4-master');
const root_directory = fullpath(system.exec_dir + '../web/synchronet-web-v4-master/root');
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

writeln('Downloading ' + zip_url + ' to ' + download_target + ' ...');
if (!download(zip_url, download_target)) {
    writeln('Download of ' + zip_url + ' failed. Exiting.');
    exit();
}

writeln('Extracting ' + download_target + ' to ' + extract_dir + ' ...');
if (!extract(download_target, extract_dir)) {
    writeln('Extraction of ' + download_target + ' failed. Exiting.');
    exit();
}

writeln('Updating sbbs.ini ...');
if (!update_sbbs_ini(root_directory, error_directory)) {
    writeln('Failed to update sbbs.ini. Exiting.');
    exit();
}

modopts_web = get_settings(modopts_web);

writeln('Updating modopts.ini ...');
if (!update_modopts_ini(modopts_web)) {
    writeln('Failed to update modopts.ini. Exiting.');
    exit();
}
