load('http.js');

function download(url, target) {
	const http = new HTTPRequest();
	const zip = http.Get(url);
	const f = new File(target);
	f.open('wb');
	f.write(zip);
	f.close();
}

function extract(file, target) {
	if (!file_exists(install_dir)) mkdir(install_dir);
	system.exec(system.exec_dir + 'unzip -u ' + download_target + ' -d ' + install_dir);
}

function update_sbbs_ini(root_directory, error_directory) {
	file_backup(system.ctrl_dir + 'sbbs.ini');
	const f = new File(system.ctrl_dir + 'sbbs.ini');
	f.open('r+');
	f.iniSetValue('Web', 'RootDirectory', root_directory);
	f.iniSetValue('Web', 'ErrorDirectory', error_directory);
	f.close();
}

function update_modopts_ini(modopts) {
	file_backup(system.ctrl_dir + 'modopts.ini');
	const f = new File(system.ctrl_dir + 'modopts.ini');
	f.open('r+');
	f.iniSetObject('web', modopts);
	f.close();
}

const zip_url = 'https://codeload.github.com/echicken/synchronet-web-v4/zip/master';
const download_target = system.temp_dir + 'webv4.zip';
const extract_dir = fullpath(system.exec_dir + '../web');
const install_dir = fullpath(extract_dir + '/synchronet-web-v4-master');
const root_directory = fullpath(system.exec_dir + '../web/synchronet-web-v4-master/root');
const error_directory = fullpath(root_directory + '/errors');
const modopts_web = {
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

download(zip_url, download_target);
extract(download_target, extract_dir);
update_sbbs_ini(root_directory, error_directory);
update_modopts_ini(modopts_web);
