var last_run = 0;
var frequency = 15;

function cycle() {
    if (user.alias === settings.guest) return;
    if (time() - last_run <= frequency) return;
    last_run = time();
    const tg = system.get_telegram(user.number);
    if (tg !== null) emit({ event: 'telegram'
		, data: JSON.stringify(html_encode(tg.replace('\7', '').replace(/\1./g, '')
			, /* ex-ascii */true
			, /* white-space */false
			, /* ansi */false
			, /* ctrl_a */false))
		});
}

this;
