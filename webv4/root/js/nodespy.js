var spy_nodes = [];

// Check if we're targeting a specific node
const urlParams = new URLSearchParams(window.location.search);
const targetNode = parseInt(urlParams.get('node'), 10);

// Create an fTelnet instance for each node
function create_ftelnet_instances() {
    // Loop through the node range
    for (var node = 1; node <= system_nodes; node++) {
        if (!targetNode || (targetNode === node)) {
            // Build a node-specific link
            var nodeUrl = location.href;
            if (!nodeUrl.includes("&node=")) {
                nodeUrl += '&node=' + node;
            }
            
            // Build a div containing the node status, keyboard checkbox, and fTelnet instance
            $('#ClientsWrapper').append(`
                <div id="Client${node}" class="${targetNode ? '' : 'col-xl-6'}">
                <h4 style="text-align: center;">
                    <a href="${nodeUrl}">Node ${node}</a> - 
                    <span id="Status${node}">Status Unknown</span> &nbsp; &nbsp; 
                    <span style="white-space: nowrap;">
                    <input type="checkbox" class="keyboard-input" id="chkKeyboard${node}" value="${node}" /> 
                    <label class="normal" for="chkKeyboard${node}">Enable Keyboard Input</label>
                    </span>
                </h4>
                <div id="fTelnetContainer${node}" class="fTelnetContainer"></div>
                </div>
            `);

            // Apply a clearfix after every other node div, to ensure the expected grid layout is followed even if some node divs are taller than others
            // Addresses https://gitlab.synchro.net/main/sbbs/-/issues/683
            if (node % 2 == 0) {
                $('#ClientsWrapper').append('<div class="clearfix visible-xl-block"></div>');
            }

            // And initialize a new fTelnet instance
            var Options = new fTelnetOptions();
            Options.AllowModernScrollback = false;
            Options.BareLFtoCRLF = false;
            Options.BitsPerSecond = 57600;
            Options.ConnectionType = 'telnet';
            Options.Emulation = 'ansi-bbs';
            Options.Enter = '\r';
            Options.Font = 'CP437';
            Options.ForceWss = false;
            Options.Hostname = 'unused';
            Options.LocalEcho = false;
            Options.Port = 11235;
            Options.ScreenColumns = 80;
            Options.ScreenRows = 25;
            Options.SplashScreen = ' ';
            spy_nodes[node] = {
                charset: 'CP437',
                ftelnet: new fTelnetClient('fTelnetContainer' + node, Options),
            };
        }
    }
}

// Setup the keyboard handler
function init_keyboard_handler() {
    // Handle clicks on the keyboard checkboxes
    $('.keyboard-input').click(function() {
        var selectedNode = this.value;
        
        // Highlight the new selected div, and un-highlight+un-check the other divs and checkboxes
        for (var node in spy_nodes) {
            if ((node === selectedNode) && this.checked) {
                $('#Client' + node).addClass('active');
            } else {
                $('#Client' + node).removeClass('active');
                $('.keyboard-input[value=' + node + ']').removeAttr('checked');
            }
        }
    });
    
    // Handle keydown event, which is where control keys and special keys are handled
    // Code is copy/pasted (with slight modifications) from fTelnet
    window.addEventListener('keydown', function (ke) {
        var node = $('.keyboard-input:checked').attr('value');
        if (node) {
            var keyString = '';
        
            if (ke.ctrlKey) {
                // Handle control + letter keys
                if ((ke.keyCode >= 65) && (ke.keyCode <= 90)) {
                    keyString = String.fromCharCode(ke.keyCode - 64);
                } else if ((ke.keyCode >= 97) && (ke.keyCode <= 122)) {
                    keyString = String.fromCharCode(ke.keyCode - 96);
                }
            } else {
                switch (ke.keyCode) {
                    // Handle special keys
                    case KeyboardKeys.BACKSPACE: keyString = '\b'; break;
                    case KeyboardKeys.DELETE: keyString = '\x7F'; break;
                    case KeyboardKeys.DOWN: keyString = '\x1B[B'; break;
                    case KeyboardKeys.END: keyString = '\x1B[K'; break;
                    case KeyboardKeys.ENTER: keyString = '\r\n'; break;
                    case KeyboardKeys.ESCAPE: keyString = '\x1B'; break;
                    case KeyboardKeys.F1: keyString = '\x1BOP'; break;
                    case KeyboardKeys.F2: keyString = '\x1BOQ'; break;
                    case KeyboardKeys.F3: keyString = '\x1BOR'; break;
                    case KeyboardKeys.F4: keyString = '\x1BOS'; break;
                    case KeyboardKeys.F5: keyString = '\x1BOt'; break;
                    case KeyboardKeys.F6: keyString = '\x1B[17~'; break;
                    case KeyboardKeys.F7: keyString = '\x1B[18~'; break;
                    case KeyboardKeys.F8: keyString = '\x1B[19~'; break;
                    case KeyboardKeys.F9: keyString = '\x1B[20~'; break;
                    case KeyboardKeys.F10: keyString = '\x1B[21~'; break;
                    case KeyboardKeys.F11: keyString = '\x1B[23~'; break;
                    case KeyboardKeys.F12: keyString = '\x1B[24~'; break;
                    case KeyboardKeys.HOME: keyString = '\x1B[H'; break;
                    case KeyboardKeys.INSERT: keyString = '\x1B@'; break;
                    case KeyboardKeys.LEFT: keyString = '\x1B[D'; break;
                    case KeyboardKeys.PAGE_DOWN: keyString = '\x1B[U'; break;
                    case KeyboardKeys.PAGE_UP: keyString = '\x1B[V'; break;
                    case KeyboardKeys.RIGHT: keyString = '\x1B[C'; break;
                    case KeyboardKeys.SPACE: keyString = ' '; break;
                    case KeyboardKeys.TAB: keyString = '\t'; break;
                    case KeyboardKeys.UP: keyString = '\x1B[A'; break;
                }
            }

            // If we have a keyString, then publish it so sbbs sees it as input
            if (keyString) {
                mqtt_publish('sbbs/' + system_qwk_id + '/node/' + node + '/input', keyString);
            }

            // If we have a keyString, or CTRL is being held, prevent further handling (so the keypress event won't be hit next)
            if ((keyString) || (ke.ctrlKey)) {
                ke.preventDefault();
            }
        }
    });
    
    // Handle keypress event, which is where standard keys are handled
    // Code is copy/pasted (with slight modifications) from fTelnet
    window.addEventListener('keypress', function (ke) {
        var node = $('.keyboard-input:checked').attr('value');
        if (node) {
            if (ke.charCode >= 33) {
                mqtt_publish('sbbs/' + system_qwk_id + '/node/' + node + '/input', String.fromCharCode(ke.charCode));
            }
        }
    });
}

// Log a message to each of the fTelnet instances
function log_ftelnet(message) {
    for (var node in spy_nodes) {
        spy_nodes[node].ftelnet._Crt.WriteLn(message);
    }
}

function mqtt_message(topic, message, packet) {
    var match = topic.match(/node[\/](\d*)([\/](output|terminal))?/);
    var node = match[1];
    var messageType = match[2];
    
    switch (messageType) {
        case undefined:
            // message contains the node's current activity (ie waiting for caller, or user is running an external, etc)
            $('#Status' + node).html(message.toString());
            break;
        
        case '/output':
            // message contains the raw output being sent to the user, so write it to the fTelnet instance
            var buf = new Uint8Array(message);

            var str = '';
            for (var i = 0; i < message.length; i++) {
                str += String.fromCharCode(buf[i]);
            }

            // UTF-8 decode, if necessary
            if (spy_nodes[node].charset === 'UTF-8') {
                str = utf8_cp437(str);
            }

            spy_nodes[node].ftelnet._Ansi.Write(str);
            break;
        
        case '/terminal':
            // message contains tab-separated terminal information, most importantly the column and row count as the first
            // and second elements, so resize fTelnet and tell it to reload the best font based on the new size.
            var arr = message.toString().split('\t');
            spy_nodes[node].ftelnet._Crt.SetScreenSize(parseInt(arr[0], 10), parseInt(arr[1], 10));
            spy_nodes[node].ftelnet._Crt.SetFont(spy_nodes[node].ftelnet._Crt.Font.Name); 
            spy_nodes[node].charset = arr[4];
            // TODOX When arr[4] is 'CBM-ASCII' things get messy -- did I add support for PETSCII to fTelnet?
            break;
    }        
}

// Add an onload handler to setup the node spy
window.addEventListener('load', (event) => {
    create_ftelnet_instances();
    init_keyboard_handler();
    
    var topics = [];
    for (var node in spy_nodes) {
        topics.push('sbbs/' + system_qwk_id + '/node/' + node + '');
        topics.push('sbbs/' + system_qwk_id + '/node/' + node + '/output');
        topics.push('sbbs/' + system_qwk_id + '/node/' + node + '/terminal');
    }
    mqtt_connect(topics, mqtt_message, log_ftelnet);
});
