load(settings.web_lib + 'forum.js');

var last_run = 0;
var frequency = 30;
var last_count = 0;

function cycle() {
    if (user.number < 1 || user.alias == settings.guest) return;
    if (time() - last_run <= frequency) return;
    last_run = time();
    const count = user.stats.mail_waiting;
    if (count > 0 || (count == 0 && last_count > 0)) {
        emit({ event: 'mail', data: JSON.stringify({ count: count })});
    }
    last_count = count;
}

this;
