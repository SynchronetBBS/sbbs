(function () {
    'use strict';

    var MAX_TOASTS = 4;
    var TOAST_DURATION = 5000;
    var audioHandles = {};
    var audioBufferCache = {};
    var audioContext = null;
    var audioUnlockPromise = null;
    var audioUnlockBound = false;
    var audioUnlocked = false;

    function clampNumber(value, min, max, fallback) {
        var num = parseFloat(value);
        if (isNaN(num)) return fallback;
        if (num < min) return min;
        if (num > max) return max;
        return num;
    }

    function normalizeActionName(mode) {
        switch (String(mode || '').toLowerCase()) {
            case 'keyboard': return 'showKeyboard';
            case 'fn': return 'showFn';
            case 'dpad': return 'showDpad';
            case 'dismiss':
            case 'off':
            case 'hidden':
                return 'dismiss';
            default:
                return '';
        }
    }

    function buildAssetUrl(asset) {
        var params;

        if (!asset) return '';
        if (typeof asset === 'string') return asset;
        if (typeof asset.url === 'string' && asset.url) return asset.url;
        if (!asset.scope || !asset.path) return '';

        params = new URLSearchParams();
        params.set('scope', String(asset.scope));
        params.set('path', String(asset.path));
        if (asset.code) params.set('code', String(asset.code));

        return './api/flweb-assets.ssjs?' + params.toString();
    }

    function removeToast(el) {
        if (!el || !el.parentNode) return;
        el.classList.add('chat-toast-exit');
        setTimeout(function () {
            if (el.parentNode) el.parentNode.removeChild(el);
        }, 400);
    }

    function showToast(payload, ctx) {
        var container;
        var toast;
        var content;
        var title;
        var text;
        var closeBtn;
        var duration;

        payload = payload || {};
        if (payload.hiddenOnly && ctx && ctx.terminalVisible) return false;

        container = document.getElementById('chat-toasts');
        if (!container) return false;

        while (container.children.length >= MAX_TOASTS) {
            container.removeChild(container.lastChild);
        }

        toast = document.createElement('div');
        toast.className = 'chat-toast chat-toast-enter';

        content = document.createElement('div');
        content.className = 'chat-toast-content';

        title = document.createElement('div');
        title.className = 'chat-toast-sender';
        title.textContent = payload.title || 'Terminal';

        text = document.createElement('div');
        text.className = 'chat-toast-text';
        text.textContent = payload.text || payload.message || '';

        content.appendChild(title);
        content.appendChild(text);
        toast.appendChild(content);

        closeBtn = document.createElement('button');
        closeBtn.className = 'chat-toast-close';
        closeBtn.innerHTML = '&times;';
        closeBtn.addEventListener('click', function (e) {
            e.stopPropagation();
            removeToast(toast);
        });
        toast.appendChild(closeBtn);

        container.insertBefore(toast, container.firstChild);

        requestAnimationFrame(function () {
            toast.classList.remove('chat-toast-enter');
        });

        duration = clampNumber(payload.duration, 1000, 60000, TOAST_DURATION);
        setTimeout(function () {
            removeToast(toast);
        }, duration);

        return true;
    }

    function openUrl(payload, ctx) {
        var url;
        var target;
        var popup;

        payload = payload || {};
        url = payload.url || payload.href || '';
        if (!url) return false;

        try {
            url = new URL(String(url), window.location.href).toString();
        } catch (_) {
            url = String(url);
        }

        target = String(payload.target || '_blank');

        if (target === '_self') {
            window.location.assign(url);
            return true;
        }

        try {
            popup = window.open(url, target, 'noopener,noreferrer');
            if (popup) {
                try { popup.opener = null; } catch (_) {}
                return true;
            }
        } catch (_) {}

        return showToast({
            title: payload.title || 'Open Link',
            text: (payload.text || 'Popup blocked. Open this URL manually:') + ' ' + url,
            duration: clampNumber(payload.duration, 3000, 60000, 9000),
            hiddenOnly: false
        }, ctx);
    }

    function cleanupAudioHandle(id) {
        if (!id || !audioHandles[id]) return;
        delete audioHandles[id];
    }

    function stopAudioHandle(handle) {
        if (!handle) return;
        if (handle.kind === 'buffer') {
            try { if (handle.source) handle.source.onended = null; } catch (_) {}
            try { if (handle.source) handle.source.stop(0); } catch (_) {}
            try { if (handle.source) handle.source.disconnect(); } catch (_) {}
            try { if (handle.gain) handle.gain.disconnect(); } catch (_) {}
            return;
        }
        try { if (handle.pause) handle.pause(); } catch (_) {}
        try { handle.currentTime = 0; } catch (_) {}
    }

    function stopAudio(payload) {
        var id;
        var handleId;

        payload = payload || {};
        id = payload.id ? String(payload.id) : '';

        if (id) {
            if (!audioHandles[id]) return false;
            stopAudioHandle(audioHandles[id]);
            cleanupAudioHandle(id);
            return true;
        }

        for (handleId in audioHandles) {
            if (!audioHandles.hasOwnProperty(handleId)) continue;
            stopAudioHandle(audioHandles[handleId]);
            cleanupAudioHandle(handleId);
        }

        return true;
    }

    function ensureAudioContext() {
        var Ctor;
        if (audioContext) return audioContext;
        Ctor = window.AudioContext || window.webkitAudioContext;
        if (!Ctor) return null;
        try {
            audioContext = new Ctor();
        } catch (_) {
            audioContext = null;
        }
        return audioContext;
    }

    function removeAudioUnlockListeners() {
        if (!audioUnlockBound) return;
        ['pointerdown', 'touchstart', 'mousedown', 'keydown'].forEach(function (type) {
            window.removeEventListener(type, unlockAudioFromGesture, true);
        });
        audioUnlockBound = false;
    }

    function unlockAudioFromGesture() {
        unlockAudio();
    }

    function installAudioUnlockListeners() {
        if (audioUnlockBound) return;
        ['pointerdown', 'touchstart', 'mousedown', 'keydown'].forEach(function (type) {
            window.addEventListener(type, unlockAudioFromGesture, true);
        });
        audioUnlockBound = true;
    }

    function decodeAudioDataCompat(ctx, arrayBuffer) {
        return new Promise(function (resolve, reject) {
            var cloned = arrayBuffer.slice ? arrayBuffer.slice(0) : arrayBuffer;
            var settled = false;
            function onResolve(buffer) {
                if (settled) return;
                settled = true;
                resolve(buffer);
            }
            function onReject(err) {
                if (settled) return;
                settled = true;
                reject(err);
            }
            try {
                var ret = ctx.decodeAudioData(cloned, onResolve, onReject);
                if (ret && typeof ret.then === 'function') ret.then(onResolve, onReject);
            } catch (err) {
                onReject(err);
            }
        });
    }

    function unlockAudio() {
        var ctx = ensureAudioContext();
        if (!ctx) return Promise.resolve(false);
        if (audioUnlocked && ctx.state === 'running') return Promise.resolve(true);
        if (audioUnlockPromise) return audioUnlockPromise;

        audioUnlockPromise = Promise.resolve()
            .then(function () {
                if (ctx.state === 'suspended' && typeof ctx.resume === 'function') {
                    return ctx.resume();
                }
            })
            .then(function () {
                var buffer = ctx.createBuffer(1, 1, 22050);
                var source = ctx.createBufferSource();
                source.buffer = buffer;
                source.connect(ctx.destination);
                source.start(0);
                source.disconnect();
                audioUnlocked = ctx.state === 'running';
                if (audioUnlocked) removeAudioUnlockListeners();
                return audioUnlocked;
            })
            .catch(function (err) {
                console.warn('[flweb] audio unlock failed:', err && err.message ? err.message : err);
                return false;
            })
            .then(function (result) {
                audioUnlockPromise = null;
                return result;
            });

        return audioUnlockPromise;
    }

    function getDecodedAudioBuffer(src) {
        var ctx = ensureAudioContext();
        if (!ctx) return Promise.reject(new Error('Web Audio unavailable'));
        if (audioBufferCache[src]) return audioBufferCache[src];

        audioBufferCache[src] = fetch(src, { credentials: 'same-origin' })
            .then(function (response) {
                if (!response.ok) throw new Error('HTTP ' + response.status);
                return response.arrayBuffer();
            })
            .then(function (arrayBuffer) {
                return decodeAudioDataCompat(ctx, arrayBuffer);
            })
            .catch(function (err) {
                delete audioBufferCache[src];
                throw err;
            });

        return audioBufferCache[src];
    }

    function playAudioViaBuffer(payload, src, id) {
        var ctx = ensureAudioContext();
        if (!ctx) return Promise.reject(new Error('Web Audio unavailable'));

        return unlockAudio().then(function (unlocked) {
            if (!unlocked && ctx.state !== 'running') {
                throw new Error('Audio context not unlocked');
            }
            return getDecodedAudioBuffer(src);
        }).then(function (buffer) {
            var source = ctx.createBufferSource();
            var gain = ctx.createGain();
            var handle;
            var loopStartSec = clampNumber(payload.loopStartSec, 0, buffer.duration, 0);
            var loopEndSec = clampNumber(payload.loopEndSec, 0, buffer.duration, buffer.duration);

            gain.gain.value = clampNumber(payload.volume, 0, 1, 1);
            source.buffer = buffer;
            source.loop = !!payload.loop;
            if (source.loop && loopEndSec > loopStartSec) {
                source.loopStart = loopStartSec;
                source.loopEnd = loopEndSec;
            }
            source.connect(gain);
            gain.connect(ctx.destination);

            handle = { kind: 'buffer', source: source, gain: gain };
            source.onended = function () {
                cleanupAudioHandle(id);
            };

            if (id) audioHandles[id] = handle;
            source.start(0);
            return true;
        });
    }

    function playAudioViaElement(payload, src, id) {
        var audio;
        var playPromise;

        audio = new Audio(src);
        audio.preload = 'auto';
        audio.loop = !!payload.loop;
        audio.volume = clampNumber(payload.volume, 0, 1, 1);

        audio.addEventListener('ended', function () {
            cleanupAudioHandle(id);
        });
        audio.addEventListener('error', function () {
            cleanupAudioHandle(id);
        });

        if (id) audioHandles[id] = audio;

        playPromise = audio.play();
        if (playPromise && typeof playPromise.catch === 'function') {
            return playPromise.then(function () {
                return true;
            });
        }
        return Promise.resolve(true);
    }

    function playAudio(payload) {
        var src;
        var id;

        payload = payload || {};
        src = buildAssetUrl(payload.asset || payload.src || payload.url);
        if (!src) return false;

        id = payload.id ? String(payload.id) : null;
        if (id && audioHandles[id]) {
            stopAudioHandle(audioHandles[id]);
            cleanupAudioHandle(id);
        }

        playAudioViaBuffer(payload, src, id).catch(function (err) {
            console.warn('[flweb] buffer audio failed, falling back:', err && err.message ? err.message : err);
            return playAudioViaElement(payload, src, id);
        }).catch(function (err) {
            console.warn('[flweb] audio.play failed:', err && err.message ? err.message : err);
            cleanupAudioHandle(id);
        });

        return true;
    }

    function normalizeWaveform(value) {
        var mode = String(value || '').toLowerCase();
        if (mode === 'sine' || mode === 'square' || mode === 'triangle' || mode === 'sawtooth') {
            return mode;
        }
        return 'square';
    }

    function playOscillatorTone(id, frequencyHz, durationMs, volume, waveform) {
        var ctx = ensureAudioContext();
        var handle;
        if (!ctx) return Promise.reject(new Error('Web Audio unavailable'));

        return unlockAudio().then(function (unlocked) {
            var now;
            var durSec;
            var osc;
            var gain;
            var maxVol;

            if (!unlocked && ctx.state !== 'running') {
                throw new Error('Audio context not unlocked');
            }

            now = ctx.currentTime;
            durSec = Math.max(0.01, durationMs / 1000);
            osc = ctx.createOscillator();
            gain = ctx.createGain();
            maxVol = Math.max(0.0001, volume);

            osc.type = normalizeWaveform(waveform);
            osc.frequency.setValueAtTime(Math.max(20, frequencyHz), now);

            gain.gain.setValueAtTime(0.0001, now);
            gain.gain.exponentialRampToValueAtTime(maxVol, now + 0.004);
            gain.gain.exponentialRampToValueAtTime(0.0001, now + durSec);

            osc.connect(gain);
            gain.connect(ctx.destination);

            handle = { kind: 'buffer', source: osc, gain: gain };
            osc.onended = function () {
                cleanupAudioHandle(id);
            };

            if (id) audioHandles[id] = handle;
            osc.start(now);
            osc.stop(now + durSec + 0.01);
            return true;
        });
    }

    function playZztNote(payload) {
        var id;
        var frequencyHz;
        var durationMs;
        var volume;
        var waveform;

        payload = payload || {};
        id = payload.id ? String(payload.id) : 'zzt-main';
        frequencyHz = clampNumber(payload.frequencyHz, 20, 20000, 440);
        durationMs = clampNumber(payload.durationMs, 10, 10000, 60);
        volume = clampNumber(payload.volume, 0, 1, 0.18);
        waveform = payload.waveform || 'square';

        if (id && audioHandles[id]) {
            stopAudioHandle(audioHandles[id]);
            cleanupAudioHandle(id);
        }

        playOscillatorTone(id, frequencyHz, durationMs, volume, waveform).catch(function (err) {
            console.warn('[flweb] audio.zzt.note failed:', err && err.message ? err.message : err);
            cleanupAudioHandle(id);
        });

        return true;
    }

    function playZztDrum(payload) {
        var drum;
        var durationMs;
        var volume;
        var id;
        var freq;
        var waveform;
        var drumFreqs = [3200, 1200, 2400, 880, 2200, 1400, 960, 700, 1500, 440];

        payload = payload || {};
        id = payload.id ? String(payload.id) : 'zzt-main';
        drum = Math.floor(clampNumber(payload.drumId, 0, 9, 0));
        durationMs = clampNumber(payload.durationMs, 10, 10000, 70);
        volume = clampNumber(payload.volume, 0, 1, 0.25);
        freq = drumFreqs[drum] || 880;
        freq = freq + ((Math.random() * 2 - 1) * (freq * 0.12));
        waveform = (drum === 4 || drum === 9) ? 'sawtooth' : 'square';

        if (id && audioHandles[id]) {
            stopAudioHandle(audioHandles[id]);
            cleanupAudioHandle(id);
        }

        playOscillatorTone(id, freq, durationMs, volume, waveform).catch(function (err) {
            console.warn('[flweb] audio.zzt.drum failed:', err && err.message ? err.message : err);
            cleanupAudioHandle(id);
        });

        return true;
    }

    function say(payload) {
        var utter;
        var voices;
        var i;
        var voiceMatch;
        var candidates;
        var j;

        payload = payload || {};
        if (!window.speechSynthesis || !payload.text) return false;
        if (payload.interrupt !== false) {
            try { window.speechSynthesis.cancel(); } catch (_) {}
        }

        utter = new SpeechSynthesisUtterance(String(payload.text));
        if (payload.lang) utter.lang = String(payload.lang);
        utter.rate = clampNumber(payload.rate, 0.1, 10, 1);
        utter.pitch = clampNumber(payload.pitch, 0, 2, 1);
        utter.volume = clampNumber(payload.volume, 0, 1, 1);

        voices = window.speechSynthesis.getVoices();
        voiceMatch = null;

        candidates = [];
        if (payload.voice) {
            candidates.push(String(payload.voice));
        }
        if (payload.voiceCandidates && payload.voiceCandidates.length) {
            for (j = 0; j < payload.voiceCandidates.length; j += 1) {
                candidates.push(String(payload.voiceCandidates[j]));
            }
        }

        for (j = 0; j < candidates.length && !voiceMatch; j += 1) {
            for (i = 0; i < voices.length; i += 1) {
                if (voices[i].name.toLowerCase().indexOf(candidates[j].toLowerCase()) !== -1) {
                    voiceMatch = voices[i];
                    break;
                }
            }    
        }

        if (!voiceMatch && utter.lang) {
            for (i = 0; i < voices.length; i += 1) {
                if (String(voices[i].lang || '').toLowerCase().indexOf(String(utter.lang).toLowerCase()) === 0) {
                    voiceMatch = voices[i];
                    break;
                }
            }
        }

        if (voiceMatch) utter.voice = voiceMatch;

        window.speechSynthesis.speak(utter);
        return true;
    }

    function handleController(action, payload, ctx) {
        var modeAction;

        payload = payload || {};
        if (!ctx || typeof ctx.sendToIframe !== 'function') return false;

        if (action === 'controller.mode') {
            modeAction = normalizeActionName(payload.mode);
            if (!modeAction) return false;
            ctx.sendToIframe({ cmd: 'mobileInput', action: modeAction });
            return true;
        }

        if (action === 'controller.mapping') {
            if (!payload.mapping) return false;
            ctx.sendToIframe({
                cmd: 'mobileInput',
                action: 'setMapping',
                mapping: String(payload.mapping)
            });
            return true;
        }

        if (action === 'controller.profile') {
            if (payload.mapping) {
                ctx.sendToIframe({
                    cmd: 'mobileInput',
                    action: 'setMapping',
                    mapping: String(payload.mapping)
                });
            }
            if (payload.mode) {
                modeAction = normalizeActionName(payload.mode);
                if (modeAction) {
                    ctx.sendToIframe({ cmd: 'mobileInput', action: modeAction });
                }
            }
            return true;
        }

        return false;
    }

    function handleRadioPlay(payload) {
        var file = payload && (payload.file || payload.filename);
        if (!file) {
            console.warn('[flweb] radio.play: no file specified');
            return false;
        }
        if (!window.sbbsRadio) {
            console.warn('[flweb] radio.play: sbbsRadio not available');
            showToast({ title: 'Radio', text: 'Radio player not loaded yet. Open the radio first.' });
            return false;
        }
        if (typeof window.sbbsRadio.playByFile === 'function') {
            window.sbbsRadio.playByFile(file);
            return true;
        }
        console.warn('[flweb] radio.play: playByFile not available on sbbsRadio');
        return false;
    }

    function handleRadioStop() {
        if (window.sbbsRadio && typeof window.sbbsRadio.togglePlay === 'function' && window.sbbsRadio.isPlaying) {
            window.sbbsRadio.togglePlay();
            return true;
        }
        return false;
    }

    function handleTerminalUi(msg, ctx) {
        var action = msg && msg.action;
        var payload = msg && msg.payload ? msg.payload : msg;

        switch (action) {
            case 'alert':
                window.alert((msg && msg.message) || (payload && payload.message) || 'sent from terminal');
                return true;
            case 'toast.show':
                return showToast(payload, ctx);
            case 'audio.play':
                return playAudio(payload);
            case 'audio.stop':
                return stopAudio(payload);
            case 'audio.zzt.note':
                return playZztNote(payload);
            case 'audio.zzt.drum':
                return playZztDrum(payload);
            case 'audio.zzt.stop':
                return stopAudio(payload);
            case 'speech.say':
                return say(payload);
            case 'url.open':
                return openUrl(payload, ctx);
            case 'controller.mode':
            case 'controller.mapping':
            case 'controller.profile':
                return handleController(action, payload, ctx);
            case 'radio.play':
                return handleRadioPlay(payload);
            case 'radio.stop':
                return handleRadioStop();
            case 'bridge.probe':
                /* Handled in terminal-iframe.html via WebSocket I/O.
                   The parent gets a copy of the postMessage for logging only. */
                console.log('[flweb] bridge.probe received (handled by iframe)');
                return true;
            default:
                console.warn('[flweb] unhandled terminal-ui action:', action, payload);
                return false;
        }
    }

    window.FLWeb = {
        buildAssetUrl: buildAssetUrl,
        handleTerminalUi: handleTerminalUi,
        unlockAudio: unlockAudio,
        playAudio: playAudio,
        stopAudio: stopAudio,
        openUrl: openUrl,
        showToast: showToast,
        say: say
    };

    installAudioUnlockListeners();
})();
