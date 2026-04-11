/* experiment3.js
 *
 * Small FLWEB protocol test pad.
 * Lets us trigger browser-side actions directly from a door so testing the
 * protocol does not depend on slower shell integration points.
 */

'use strict';

var flweb = {};
load(flweb, system.mods_dir + 'load/flweb.js');

var statusMessage = 'Ready.';

function drawScreen() {
    console.clear();
    console.print('Experiment 3: FLWEB Test Pad\r\n');
    console.print('============================\r\n\r\n');
    console.print('A) Alert\r\n');
    console.print('T) Toast\r\n');
    console.print('P) Play shared audio: youGotmail.mp3\r\n');
    console.print('M) Mail demo: toast + youGotmail.mp3\r\n');
    console.print('J) Play NBA_JAM main theme (xtrn asset)\r\n');
    console.print('S) Speech demo\r\n');
    console.print('K) Show mobile keyboard\r\n');
    console.print('F) Show fn keys\r\n');
    console.print('D) Show d-pad\r\n');
    console.print('X) Dismiss mobile controls\r\n');
    console.print('L) Set controller mapping: lord\r\n');
    console.print('R) Set controller profile: demo (dpad + lord)\r\n');
    console.print('B) Bridge detect (probe for web terminal)\r\n');
    console.print('W) Radio play (send radio.play for a test track)\r\n');
    console.print('Q) Quit\r\n\r\n');
    console.print('Use this to test FLWEB behavior directly from a door without\r\n');
    console.print('waiting on shell integration paths.\r\n\r\n');
    console.print('Last action: ' + statusMessage + '\r\n\r\n');
    console.print('Selection: ');
}

function setStatus(message) {
    statusMessage = message;
}

function emitAlertSequence() {
    flweb.alert('sent from terminal');
    setStatus('Sent alert action.');
}

function emitToastSequence() {
    flweb.toast('Experiment 3', 'Toast from terminal', {
        duration: 5000,
        hiddenOnly: false
    });
    setStatus('Sent toast action.');
}

function emitAudioSequence() {
    flweb.playAudio(flweb.sharedAsset('youGotmail.mp3'), {
        id: 'experiment3-mail',
        volume: 1
    });
    setStatus('Sent shared audio action for youGotmail.mp3.');
}

function emitMailDemoSequence() {
    flweb.toast('Incoming Mail', 'New e-mail from Experiment 3 <test@example.com>', {
        duration: 6000,
        hiddenOnly: false
    });
    flweb.playAudio(flweb.sharedAsset('youGotmail.mp3'), {
        id: 'experiment3-mail',
        volume: 1
    });
    setStatus('Sent combined mail demo toast + audio.');
}

function emitNbaJamAudioSequence() {
    flweb.playAudio(flweb.xtrnAsset('01 - Main Theme - Jon Hey.mp3', 'NBA_JAM'), {
        id: 'experiment3-nba-jam',
        volume: 0.4,
        loop: true
    });
    setStatus('Sent xtrn audio action for NBA_JAM main theme.');
}

function emitSpeechSequence() {
    flweb.say('Experiment three speech test.', {
        rate: 1,
        volume: 1
    });
    setStatus('Sent speech action.');
}

function emitControllerMode(mode, label) {
    flweb.controllerMode(mode);
    setStatus('Sent controller mode: ' + label + '.');
}

function emitMappingSequence() {
    flweb.controllerMapping('lord');
    setStatus('Sent controller mapping: lord.');
}

function emitProfileSequence() {
    flweb.controllerProfile('demo', {
        mode: 'dpad',
        mapping: 'lord'
    });
    setStatus('Sent controller profile: demo (dpad + lord).');
}

function emitBridgeDetect() {
    var result = flweb.bridgeDetect({ force: true });
    if (result.available) {
        setStatus('Bridge DETECTED!  caps: ' + JSON.stringify(result.caps));
    } else {
        setStatus('Bridge NOT detected (no response within timeout).');
    }
}

function emitRadioPlay() {
    flweb.radioPlay('80s_Pop_hm_derdoc.mp3', { title: '80s Pop' });
    setStatus('Sent radio.play for 80s_Pop_hm_derdoc.mp3');
}

while (bbs.online && !js.terminated) {
    drawScreen();

    var key = console.getkey();
    if (!key) {
        continue;
    }

    key = key.toUpperCase();
    if (key === 'A') {
        emitAlertSequence();
    } else if (key === 'T') {
        emitToastSequence();
    } else if (key === 'P') {
        emitAudioSequence();
    } else if (key === 'M') {
        emitMailDemoSequence();
    } else if (key === 'J') {
        emitNbaJamAudioSequence();
    } else if (key === 'S') {
        emitSpeechSequence();
    } else if (key === 'K') {
        emitControllerMode('keyboard', 'keyboard');
    } else if (key === 'F') {
        emitControllerMode('fn', 'fn');
    } else if (key === 'D') {
        emitControllerMode('dpad', 'dpad');
    } else if (key === 'X') {
        emitControllerMode('dismiss', 'dismiss');
    } else if (key === 'L') {
        emitMappingSequence();
    } else if (key === 'R') {
        emitProfileSequence();
    } else if (key === 'B') {
        emitBridgeDetect();
    } else if (key === 'W') {
        emitRadioPlay();
    } else if (key === 'Q' || key === '\x1b') {
        break;
    } else {
        setStatus('Ignored key: ' + JSON.stringify(key));
    }
}
