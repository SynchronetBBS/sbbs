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
		return await fetch(url, init).then(res => res.json());
	} catch (err) {
		console.log('Error on fetch', url, init);
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

async function login(evt) {
	if ($('#input-username').val() == '' || $('#input-password').val() == '') {
		return;
	}
	if (typeof evt !== 'undefined') evt.preventDefault();
	const res = await v4_post('./api/auth.ssjs', {
		username: $('#input-username').val(),
		password: $('#input-password').val()
	});
	if (res.authenticated) {
		window.location.reload(true);
	} else {
		$('#login-form').append('<p class="text-danger">Login failed</p>');
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

function sendTelegram(alias) {
    function send_tg(evt) {
        if (typeof evt !== 'undefined') evt.preventDefault();
		v4_post('./api/system.ssjs', { call: 'send-telegram', user: alias, telegram: $('#telegram').val() });
        $('#popUpModal').modal('hide');
    }
	$('#popUpModalTitle').html('Send a telegram to ' + alias);
	$('#popUpModalBody').html(
        '<form id="send-telegram-form">'
		+ '<input type="text" class="form-control" placeholder="My message" name="telegram" id="telegram">'
        + '<input type="submit" value="submit" class="hidden">'
        + '</form>'
	);
    $('#send-telegram-form').submit(send_tg);
	$('#popUpModalActionButton').click(send_tg);
    $('#popUpModalActionButton').show();
	$('#popUpModal').modal('show');
}

function registerEventListener(scope, callback, params) {
	params = Object.keys(params || {}).reduce(function (a, c) {
		a += '&' + c + '=' + params[c];
		return a;
	}, '');
	_sbbs_events[scope] = {
		qs: 'subscribe=' + scope + params,
		callback: callback
	};
}

window.onload =	function () {

	$('#button-logout').click(logout);
	$('#button-login').click(login);
	$('#form-login').submit(login);

	$('#popUpModal').on('hidden.bs.modal', function (e) {
		$('#popUpModalActionButton').off('click');
		$('#popUpModalTitle').empty();
		$('#popUpModalBody').empty();
	});
	$("#popUpModalCloseButton").click(function () {
		$('#popUpModal').modal('hide');
	});

	setTimeout(scrollUp, 25);
	window.onhashchange = scrollUp;

	if ($('#button-logout').length > 0) {

		registerEventListener('mail', function (e) {
            const data = JSON.parse(e.data);
            if (typeof data.count != 'number') return;
            $('#badge-unread-mail').text(data.count < 1 ? '' : data.count);
            $('#badge-unread-mail-inner').text(data.count < 1 ? '' : data.count);
		});
		
		registerEventListener('telegram', function (e) {
            const tg = JSON.parse(e.data).replace(/\1./g, '').replace(
                /\r?\n/g, '<br>'
            );
            $('#popUpModalTitle').html('New telegram(s) received');
            $('#popUpModalBody').append(tg);
            $('#popUpModalActionButton').hide();
            $('#popUpModal').modal('show');
		});

	}

	const qs = Object.keys(_sbbs_events).reduce(function (a, c, i) {
		return a + (i == 0 ? '?' : '&') + _sbbs_events[c].qs;
	}, '');

    const es = new EventSource('/api/events.ssjs' + qs);
    Object.keys(_sbbs_events).forEach(function (e) {
        es.addEventListener(e, _sbbs_events[e].callback);
    });

}
