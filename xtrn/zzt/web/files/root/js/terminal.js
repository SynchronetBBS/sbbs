/* terminal.js - fTelnet lifecycle manager (iframe proxy)
 *
 * fTelnet runs inside an iframe for browsing-context isolation:
 * its setInterval paint loop can't compete with parent page animations.
 * All terminal commands go through postMessage; status comes back the same way.
 */
(function () {
    'use strict';

    var cfg = window.sbbsConfig;
    if (!cfg.ftelnet) return;
    var BRIDGE_CONFIG_DEFAULTS = {
        lockScreenSize: false,
        columns: 80,
        rows: 25,
        minColumns: 80,
        maxColumns: 160,
        minRows: 24,
        maxRows: 60,
        enableTouchHandlers: true,
        enableWheelArrows: true,
        enableRenderSpeedPatch: true,
        enableParentAudioUnlockBridge: true,
        enableCanvasScale: true
    };

    var panel = document.getElementById('terminal-panel');
    var iframeContainer = document.getElementById('terminal-iframe-container');
    var btnToggle = document.getElementById('btn-terminal');
    var statusDot = document.getElementById('terminal-status'); // may be null now
    var btnTerminal = document.getElementById('btn-terminal');

    var iframe = null;
    var iframeReady = false;
    var pendingMessages = [];
    var isConnected = false;
    var isVisible = false;
    var initialized = false;
    var isAnimating = false;
    var isSecure = location.protocol === 'https:';
    var resizeTimer = null;
    var restoreTimer = null;
    var bridgeConfig = normalizeBridgeConfig(BRIDGE_CONFIG_DEFAULTS);
    var bridgeConfigPromise = loadBridgeConfig();

    function toInt(value, fallback, min, max) {
        var parsed = parseInt(value, 10);
        if (!isFinite(parsed) || isNaN(parsed)) parsed = fallback;
        if (isFinite(min) && parsed < min) parsed = min;
        if (isFinite(max) && parsed > max) parsed = max;
        return parsed;
    }

    function toBool(value, fallback) {
        if (value === true || value === false) return value;
        if (typeof value === 'number') return value !== 0;
        if (typeof value === 'string') {
            var lowered = value.toLowerCase();
            if (lowered === '1' || lowered === 'true' || lowered === 'yes' || lowered === 'on')
                return true;
            if (lowered === '0' || lowered === 'false' || lowered === 'no' || lowered === 'off')
                return false;
        }
        return fallback;
    }

    function normalizeBridgeConfig(input) {
        var src = input || {};
        var out = {};
        out.lockScreenSize = toBool(src.lockScreenSize, BRIDGE_CONFIG_DEFAULTS.lockScreenSize);
        out.columns = toInt(src.columns, BRIDGE_CONFIG_DEFAULTS.columns, 40, 240);
        out.rows = toInt(src.rows, BRIDGE_CONFIG_DEFAULTS.rows, 15, 120);
        out.minColumns = toInt(src.minColumns, BRIDGE_CONFIG_DEFAULTS.minColumns, 40, 240);
        out.maxColumns = toInt(src.maxColumns, BRIDGE_CONFIG_DEFAULTS.maxColumns, 40, 240);
        out.minRows = toInt(src.minRows, BRIDGE_CONFIG_DEFAULTS.minRows, 15, 120);
        out.maxRows = toInt(src.maxRows, BRIDGE_CONFIG_DEFAULTS.maxRows, 15, 120);
        out.enableTouchHandlers = toBool(src.enableTouchHandlers, BRIDGE_CONFIG_DEFAULTS.enableTouchHandlers);
        out.enableWheelArrows = toBool(src.enableWheelArrows, BRIDGE_CONFIG_DEFAULTS.enableWheelArrows);
        out.enableRenderSpeedPatch = toBool(src.enableRenderSpeedPatch, BRIDGE_CONFIG_DEFAULTS.enableRenderSpeedPatch);
        out.enableParentAudioUnlockBridge = toBool(
            src.enableParentAudioUnlockBridge,
            BRIDGE_CONFIG_DEFAULTS.enableParentAudioUnlockBridge
        );
        out.enableCanvasScale = toBool(src.enableCanvasScale, BRIDGE_CONFIG_DEFAULTS.enableCanvasScale);

        if (out.minColumns > out.maxColumns) out.minColumns = out.maxColumns;
        if (out.minRows > out.maxRows) out.minRows = out.maxRows;
        if (out.columns < out.minColumns) out.columns = out.minColumns;
        if (out.columns > out.maxColumns) out.columns = out.maxColumns;
        if (out.rows < out.minRows) out.rows = out.minRows;
        if (out.rows > out.maxRows) out.rows = out.maxRows;
        return out;
    }

    function fetchJson(url) {
        if (typeof window.v4_get === 'function') {
            return window.v4_get(url);
        }
        return fetch(url).then(function (response) { return response.json(); });
    }

    function loadBridgeConfig() {
        return fetchJson('./api/terminal-ui-config.ssjs?_=' + Date.now())
            .then(function (res) {
                var opts = res && res.options ? res.options : res;
                bridgeConfig = normalizeBridgeConfig(opts || BRIDGE_CONFIG_DEFAULTS);
                return bridgeConfig;
            })
            .catch(function () {
                bridgeConfig = normalizeBridgeConfig(BRIDGE_CONFIG_DEFAULTS);
                return bridgeConfig;
            });
    }

    /* ============================================================
     *  Responsive screen size calculation
     * ============================================================ */

    function getNavbarHeight() {
        var nav = document.querySelector('.navbar.fixed-top');
        if (nav) {
            var h = nav.getBoundingClientRect().height;
            document.documentElement.style.setProperty('--navbar-h', h + 'px');
            return h;
        }
        return 56; /* fallback */
    }
    /* Screen sizing is owned by the iframe (terminal-iframe.html)
     * where the exact pixel dimensions are known. The parent just
     * triggers resize; the iframe calculates cols/rows from the
     * CP437 font table and its own window dimensions. */

    /* ============================================================
     *  postMessage bridge
     * ============================================================ */

    function sendToIframe(msg) {
        if (iframe && iframe.contentWindow && iframeReady) {
            iframe.contentWindow.postMessage(msg, location.origin);
        } else {
            pendingMessages.push(msg);
        }
    }

    function afterNextPaint(cb) {
        if (typeof window.requestAnimationFrame === 'function') {
            window.requestAnimationFrame(cb);
        } else {
            setTimeout(cb, 0);
        }
    }

    function setPanelHidden(hidden) {
        if (!panel) return;
        panel.classList.toggle('is-hidden', hidden);
        panel.setAttribute('aria-hidden', hidden ? 'true' : 'false');
    }

    function focusTerminal() {
        if (!iframe) return;
        try { iframe.focus(); } catch (_) { }
        try {
            if (iframe.contentWindow &&
                typeof iframe.contentWindow.focus === 'function') {
                iframe.contentWindow.focus();
            }
        } catch (_) { }
        sendToIframe({ cmd: 'focus' });
    }

    function restoreTerminalViewport() {
        clearTimeout(restoreTimer);
        afterNextPaint(function () {
            if (!isVisible || !initialized || !iframeReady) return;
            if (!bridgeConfig.lockScreenSize) sendToIframe({ cmd: 'resize' });
            sendToIframe({ cmd: 'refit' });
            focusTerminal();
            restoreTimer = setTimeout(function () {
                if (!isVisible || !iframeReady) return;
                if (!bridgeConfig.lockScreenSize) sendToIframe({ cmd: 'resize' });
                sendToIframe({ cmd: 'refit' });
                focusTerminal();
            }, 180);
        });
    }

    window.addEventListener('message', function (e) {
        if (e.origin !== location.origin) return;
        var msg = e.data;
        if (!msg || !msg.type) return;

        switch (msg.type) {
            case 'loaded':
                /* iframe HTML loaded — send init config (bypasses queue).
                   cols/rows are calculated by the iframe itself. */
                bridgeConfigPromise.then(function () {
                    iframe.contentWindow.postMessage({
                        cmd: 'init',
                        config: {
                            ftelnetUrl: cfg.ftelnetUrl,
                            ftelnetSplash: cfg.ftelnetSplash,
                            hostname: cfg.hostname,
                            wsp: cfg.wsp,
                            wssp: cfg.wssp,
                            telnetPort: cfg.telnetPort,
                            rloginPort: cfg.rloginPort,
                            isLoggedIn: cfg.isLoggedIn,
                            userAlias: cfg.userAlias,
                            userPassword: cfg.userPassword,
                            isSecure: isSecure,
                            bridgeOptions: bridgeConfig
                        }
                    }, location.origin);
                });
                break;

            case 'ready':
                iframeReady = true;
                /* flush any commands queued before iframe was ready */
                while (pendingMessages.length) {
                    iframe.contentWindow.postMessage(
                        pendingMessages.shift(), location.origin);
                }
                if (isVisible) restoreTerminalViewport();
                break;

            case 'status':
                if (isConnected && !msg.connected && isVisible) {
                    setTimeout(hidePanel, 1200);
                }
                isConnected = msg.connected;
                updateStatus();
                break;

            case 'click':
                if (isVisible && !isConnected && initialized) {
                    sendToIframe({ cmd: 'connect' });
                }
                break;

            case 'error':
                console.error('[terminal] iframe error:', msg.message);
                break;

            case 'terminal-ui':
                if (window.FLWeb && typeof window.FLWeb.handleTerminalUi === 'function') {
                    window.FLWeb.handleTerminalUi(msg, {
                        terminalVisible: isVisible,
                        sendToIframe: sendToIframe,
                        showTerminal: showPanel,
                        hideTerminal: hidePanel
                    });
                } else if (msg.action === 'alert') {
                    window.alert(msg.message || 'sent from terminal');
                }
                break;
        }
    });

    /* ============================================================
     *  Resize handler
     * ============================================================ */

    function handleResize() {
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(function () {
            if (!initialized || !iframeReady) return;
            if (bridgeConfig.lockScreenSize) {
                sendToIframe({ cmd: 'refit' });
            } else {
                sendToIframe({ cmd: 'resize' });
            }
        }, 250);
    }

    function attachResizeHandler() {
        window.addEventListener('resize', handleResize);
        window.addEventListener('orientationchange', handleResize);
    }

    /* ============================================================
     *  Iframe creation
     * ============================================================ */

    function createIframe() {
        if (iframe) return;
        iframe = document.createElement('iframe');
        iframe.id = 'terminal-iframe';
        iframe.src = './terminal-iframe.html';
        iframe.style.cssText =
            'border:none;width:100%;height:100%;display:block;background:#000;';
        iframe.tabIndex = -1;
        iframe.setAttribute('allow', 'autoplay');
        iframeContainer.appendChild(iframe);
        initialized = true;
        attachResizeHandler();
    }

    /* ============================================================
     *  UI — status dot
     * ============================================================ */

    function updateStatus() {
        if (!btnTerminal) return;
        var crtLabel = document.getElementById('crt-label');

        // Three visual states tracked via classes on the button:
        //   .crt-visible  = terminal panel is showing
        //   .modem-active = WebSocket/telnet connection is live
        btnTerminal.classList.toggle('crt-visible', isVisible);
        btnTerminal.classList.toggle('modem-active', isConnected);

        // CRT label text:
        //   connected  -> "ONLINE"  (green)
        //   visible but not connected -> "CRT ON" (amber)
        //   hidden     -> "ENTER"   (amber, dimmer)
        if (crtLabel) {
            if (isConnected) {
                crtLabel.textContent = 'ONLINE';
            } else if (isVisible) {
                crtLabel.textContent = 'CRT ON';
            } else {
                crtLabel.textContent = 'ENTER';
            }
        }
    }

    /* ---------- CRT power-on animation ---------- */

    function playCrtAnimation(callback) {
        var overlay = document.createElement('div');
        overlay.className = 'crt-overlay';
        overlay.innerHTML =
            '<div class="crt-scanline"></div>' +
            '<div class="crt-bloom"></div>' +
            '<div class="crt-band" style="top:0;height:33%"></div>' +
            '<div class="crt-band" style="top:33%;height:34%"></div>' +
            '<div class="crt-band" style="top:67%;height:33%"></div>' +
            '<div class="crt-roll-band"></div>' +
            '<div class="crt-brightness"></div>';
        document.body.appendChild(overlay);

        var t = { black: 100, scanline: 40, bloom: 130,
                  unstable: 220, lock: 40, settle: 90 };

        setTimeout(function () {
            overlay.classList.add('crt-phase-scanline');
            setTimeout(function () {
                overlay.classList.remove('crt-phase-scanline');
                overlay.classList.add('crt-phase-bloom');
                overlay.style.background = 'transparent';
                if (callback) callback();
                setTimeout(function () {
                    overlay.classList.remove('crt-phase-bloom');
                    overlay.classList.add('crt-phase-unstable');
                    setTimeout(function () {
                        overlay.classList.remove('crt-phase-unstable');
                        overlay.classList.add('crt-phase-lock');
                        setTimeout(function () {
                            overlay.classList.remove('crt-phase-lock');
                            overlay.classList.add('crt-phase-settle');
                            setTimeout(function () {
                                overlay.remove();
                            }, t.settle);
                        }, t.lock);
                    }, t.unstable);
                }, t.bloom);
            }, t.scanline);
        }, t.black);
    }

    /* ---------- CRT power-off animation ---------- */

    function playCrtOffAnimation(callback) {
        var overlay = document.createElement('div');
        overlay.className = 'crt-off-overlay';
        overlay.innerHTML = '<div class="crt-off-line"></div>';
        panel.appendChild(overlay);

        void overlay.offsetHeight;
        overlay.classList.add('crt-off-flash');

        setTimeout(function () {
            overlay.classList.remove('crt-off-flash');
            overlay.classList.add('crt-off-line-show');
            setTimeout(function () {
                overlay.classList.remove('crt-off-line-show');
                overlay.classList.add('crt-off-line-fade');
                setTimeout(function () {
                    overlay.remove();
                    if (callback) callback();
                }, 200);
            }, 160);
        }, 80);
    }

    /* ---------- panel show / hide ---------- */

    function showPanel(skipAnimation) {
        if (!panel || isAnimating) return;
        // Close visualizer if open
        if (window.sbbsVisualizer && window.sbbsVisualizer.hide) window.sbbsVisualizer.hide();
        getNavbarHeight();
        document.body.classList.add('terminal-open');

        function afterShow() {
            setPanelHidden(false);
            isVisible = true;
            updateStatus();
            if (!initialized) {
                createIframe();
            } else {
                restoreTerminalViewport();
                if (!isConnected) {
                    sendToIframe({ cmd: 'connect' });
                }
            }
        }

        if (skipAnimation) {
            afterShow();
            return;
        }
        playCrtAnimation(afterShow);
    }

    function hidePanel() {
        if (!panel || !isVisible || isAnimating) return;
        isAnimating = true;
        clearTimeout(restoreTimer);

        playCrtOffAnimation(function () {
            setPanelHidden(true);
            document.body.classList.remove('terminal-open');
            try { if (iframe) iframe.blur(); } catch (_) { }
            sendToIframe({ cmd: 'blur' });
            isVisible = false;
            isAnimating = false;
            updateStatus();
        });
    }

    function togglePanel() {
        if (isVisible) hidePanel();
        else showPanel();
    }

    /* ============================================================
     *  RLogin connect
     * ============================================================ */

    function connectRLogin(username, password) {
        sendToIframe({
            cmd: 'rlogin',
            username: username,
            password: password,
            isSecure: isSecure,
            rloginPort: cfg.rloginPort
        });
    }

    /* ============================================================
     *  Event bindings
     * ============================================================ */

    if (btnToggle) btnToggle.addEventListener('click', togglePanel);


    window.addEventListener('spa:beforeNavigate', function () {
        if (isVisible) hidePanel();
    });

    document.addEventListener('spa:login', function (e) {
        if (e.detail && e.detail.username && e.detail.password) {
            if (!isVisible) showPanel();
            connectRLogin(e.detail.username, e.detail.password);
        }
    });

    document.addEventListener('spa:logout', function () {
        sendToIframe({
            cmd: 'logout',
            isSecure: isSecure,
            telnetPort: cfg.telnetPort
        });
        isConnected = false;
        updateStatus();
    });

    function launchXtrn(code) {
        if (!code) return Promise.resolve();
        return v4_get('./api/system.ssjs?call=set-xtrn-intent&code=' +
            encodeURIComponent(code)).then(function () {
            showPanel(isVisible);
            sendToIframe({ cmd: 'xtrn', code: code });
        });
    }

    /* ============================================================
     *  Public API
     * ============================================================ */

    window.sbbsTerminal = {
        show: showPanel,
        hide: hidePanel,
        toggle: togglePanel,
        connectRLogin: connectRLogin,
        launchXtrn: launchXtrn,
        isConnected: function () { return isConnected; }
    };
})();
