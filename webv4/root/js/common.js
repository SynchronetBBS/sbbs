// How often to check for unread mail, new telegrams (milliseconds)
var updateInterval = 60000;
const _sbbs_events = {};

async function v4_fetch(url, method, body) {
	const init = { method, headers: {} };
	if (method == 'POST' && body) {
		init.body = body;
		if (body instanceof URLSearchParams) {
			init.headers['Content-Type'] = 'application/x-www-form-urlencoded';
		}
	}
	try {
		const response = await fetch(url, init);
		const data = await response.json();
		return data;
	} catch (err) {
		console.error('Error on fetch', url, init);
	}
}

function v4_get(url) {
	return v4_fetch(url);
}

// May need adjustment for multipart/form-data at some point
function v4_post(url, data) {
	const fd = new URLSearchParams();
	for (let e in data) {
		if (Array.isArray(data[e])) {
			data[e].forEach(ee => fd.append(e, ee));
		} else {
			fd.append(e, data[e]);
		}
	}
	return v4_fetch(url, 'POST', fd);
}

async function v4_fetch_jsonl(url) {
	try {
		const response = await fetch(url);
		const text = await response.text();
		return text.split(/\r\n/).reduce((a, c) => {
			if (c !== '') a.push(JSON.parse(c));
			return a;
		}, []);
	} catch (err) {
		console.error('Error on fetch_jsonl', url, err);
	}
}

async function login(evt) {
	evt.preventDefault();
	const username = document.getElementById('input-username').value;
	const password = document.getElementById('input-password').value;
	if (username == '' || password == '') return;
	const lf = document.querySelector('span[data-login-failure]');
	lf.setAttribute('hidden', true);
	const res = await v4_post('./api/auth.ssjs', { username, password });
	if (res.authenticated) {
		window.location.reload();
	} else {
		lf.removeAttribute('hidden');
	}
}

async function logout() {
	const res = await v4_post('./api/auth.ssjs', { logout: true });
	if (!res.authenticated) window.location.href = '/';
}

function scrollUp() {
	if (window.location.hash === '') return;
	if ($('#navbar').length < 1) return;
	window.scrollBy(0, -document.getElementById('navbar').offsetHeight);
}

// Add a parameter to the query string
function insertParam(key, value) {
    key = encodeURIComponent(key);
    value = encodeURIComponent(value);
    var kvp = window.location.search.substr(1).split('&');
    var i = kvp.length,	x;
    while (i--) {
		x = kvp[i].split('=');
		if (x[0] !== key) continue;
		x[1] = value;
		kvp[i] = x.join('=');
		break;
    }
    if (i<0) kvp[kvp.length] = [key,value].join('=');
    window.location.search = kvp.join('&');
}

function sendTelegram(alias) {

	function sendTg(evt) {
        if (evt !== undefined) evt.preventDefault();
		v4_post('./api/system.ssjs', {
			call: 'send-telegram',
			user: alias,
			telegram: document.querySelector('input[name=telegram]').value,
		});
        $('#popUpModal').modal('hide');
    }

	const label = document.querySelector('span[data-label-send-telegram]').cloneNode(true);
	label.innerHTML = label.innerHTML.replace('%s', alias);
	const title = document.getElementById('popUpModalTitle');
	title.innerHTML = '';
	title.appendChild(label);

	const tgForm = document.getElementById('telegram-form-template').cloneNode(true);
	tgForm.id = 'telegram-form';
	document.getElementById('popUpModalBody').appendChild(tgForm);
	tgForm.onsubmit = sendTg;

	const btn = document.getElementById('popUpModalActionButton');
	btn.style.display = '';
	btn.style.visibility = '';
	btn.onclick = sendTg;
	$('#popUpModal').modal('show');

}

function registerEventListener(scope, callback, params) {
	params = Object.keys(params || {}).reduce((a, c) => `${a}&${c}=${params[c]}`, '');
	_sbbs_events[scope] = {
		qs: `subscribe=${scope}${params}`,
		callback: callback,
	};
}

function darkmodeRequested() {
	const ls = localStorage.getItem('darkSwitch');
	if (ls === "true") return true;
	if (ls === "false") return false;
	if ((window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches)) return true;
	if (document.getElementById('darkSwitch').checked) return true;
	return false;
}

document.addEventListener('DOMContentLoaded', () => {
	// originally based on dark-mode-switch by Christian Oliff
	const ds = document.getElementById('darkSwitch');
	if (ds !== null) {
		ds.checked = darkmodeRequested();
		ds.onchange = resetTheme;
		resetTheme();
		function resetTheme() {
			if (ds.checked) {
				document.body.classList.add('dark');
				localStorage.setItem('darkSwitch', true);
			} else {
				document.body.classList.remove('dark');
				localStorage.setItem('darkSwitch', false);
			}
		}
	}
	document.documentElement.style.removeProperty('display');
});

window.onload =	function () {

	const loginForm = document.getElementById('form-login');
	if (loginForm !== null) loginForm.onsubmit = login;

	$('#popUpModal').on('hidden.bs.modal', () => {
		$('#popUpModalActionButton').off('click');
		document.getElementById('popUpModalTitle').innerHTML = '';
		document.getElementById('popUpModalBody').innerHTML = '';
	});
	document.getElementById('popUpModalCloseButton').onclick = () => $('#popUpModal').modal('hide');

	setTimeout(scrollUp, 25);
	window.onhashchange = scrollUp;

	const logoutButton = document.getElementById('button-logout');
	if (logoutButton !== null) {

		logoutButton.onclick = logout;

		registerEventListener('mail', e => {
            const data = JSON.parse(e.data);
            if (typeof data.count != 'number') return;
			document.getElementById('badge-unread-mail').innerText = data.count < 1 ? '' : data.count;
			document.getElementById('badge-unread-mail-inner').innerText = data.count < 1 ? '' : data.count;
		});
		
		registerEventListener('telegram', e => {
            const tg = JSON.parse(e.data).replace(/\1./g, '').replace(/\r?\n/g, '<br>');
			const tr = document.querySelector('span[data-label-receive-telegram]').cloneNode(true);
			const mt = document.getElementById('popUpModalTitle');
			mt.innerHTML = '';
			mt.appendChild(tr);
			document.getElementById('popUpModalBody').innerHTML += tg;
			const btn = document.getElementById('popUpModalActionButton');
			btn.style.display = 'none';
			btn.style.visibility = 'hidden';
            $('#popUpModal').modal('show');
		});

	}

	const qs = Object.entries(_sbbs_events).reduce((a, c, i) => `${a}${(i === 0 ? '?' : '&')}${c[1].qs}`, '');
    const es = new EventSource(`./api/events.ssjs${qs}`);
    Object.keys(_sbbs_events).forEach(e => es.addEventListener(e, _sbbs_events[e].callback));

}
