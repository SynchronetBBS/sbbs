var client_lists = [];
    
function handle_node_message(node, messageType, message) {
    switch (messageType) {
        case undefined:
            // In “High” Publish Verbosity mode, human-readable node status messages are published directly to node/+ topics
            // Example: sbbs/MYBBS/node/1 = Bubbaboy at external program menu via telnet
            $('#nodeStatusLabel' + node).html(message);
            break;
        
        case 'output':
            // sbbs/+/node/+/output - live output to connected-terminal (for spying)
            // Ignore, we have a separate page for node spy
            break;

        case 'status':
            // sbbs/+/node/+/status - tab-delimited node status values (see load/nodedefs.js for details)
            // Example: sbbs/VERT/node/1/status = 0       0       1       65535   0       0       0       7
            // Ignore
            break;
        
        case 'terminal':
            // sbbs/+/node/+/terminal - tab-delimited current (or last) connected-terminal definition
            // Example: sbbs/VERT/node/1/terminal = 80    24      syncterm        ANSI    CP437   6       0       2005
            // Ignore
            break;

        default:
            console.log('Unhandled node message: ' + JSON.stringify({node, messageType, message}));
            break;
    }        
}

function handle_resize() {
    // Resize tabs to fit height of window
    // 51 = navbar height, 20 = navbar margin bottom, 42 = top tab height, 42 = bottom tab height
    var tabHeight = parseInt((window.innerHeight - 51 - 20 - 42 - 42) / 2, 10);
    if (tabHeight < 200) {
        tabHeight = 200;
    }
    $('div.webmonitor-panel').css('height', tabHeight + 'px');
    $('div.webmonitor-scrolling-wrapper').css('height', tabHeight + 'px');
    // 44 = height of buttons div
    $('div.webmonitor-scrolling-wrapper-with-buttons').css('height', (tabHeight - 44) + 'px');
}

function handle_server_message(server, subtopic, subtopic2, message, packet) {
    var ignoredSubtopics = ['error_count', 'served', 'state', 'version'];
    var ignoredClientSubtopics = [undefined, 'action'];

    if (ignoredSubtopics.includes(subtopic)) {
        // Ignore, not handling these subtopics at this time
        return;
    } else if ((subtopic === 'client') && ignoredClientSubtopics.includes(subtopic2)) {
        // Ignore, not handling these client subtopics at this time
        return;
    } else if (subtopic === undefined) {
        // sbbs/+/host/+/server/+ - tab-delimited status of each server is published to its server topic
        // Examples:
        //      ready		8 served
        //      ready	32/150 clients	28 served	
        //      DISCONNECTED
        // Easier to handle here for all servers, instead of individually in the switch statement below
        var status = message.split('\t')[0];
        switch (status.toLowerCase()) {
            case 'disconnected': status = 'Stopped'; break;
            case 'initializing': status = 'Starting'; break;
            case 'ready': status = 'Running'; break;
            case 'reloading': status = 'Restarting'; break;
            case 'stopped': status = 'Stopped'; break;
            case 'stopping': status = 'Stopping'; break;
            default: console.log('Unknown Server Status: ' + status); break;
        }
        $('#' + server + 'StatusButton').text('Status: ' + status);
        return;
    } else if ((subtopic === 'client') && (subtopic2 === 'list')) {
        // sbbs/+/host/+/server/+/client_list - tab-delimited details of all connected clients, one client per line
        // Examples: 
        //      20230813T134846-240	Telnet	1	Ree	::1	localhost	54400	13184
        //      20230813T151452-240	Telnet	0	<unknown user>	::1	localhost	59258	18256
        // There is a timer set to fire every second, which will read client_lists[] and update the display, hence why
        // nothing much is happening here
        client_lists[server] = message;
        return;
    } else if (subtopic === 'log') {
        // sbbs/+/host/+/server/+/log
        // Each server/+ and event sub-topics has a log child topic where all messages of all log levels (severity) will be published as well as a grandchild topic for each log level (0-7, decreasing in severity) of logged messages.
        // Easier to handle here for all servers, instead of individually in the switch statement below
        if (subtopic2 !== undefined) {
            var level = subtopic2;
            try {
                var dateTime = timestamp_to_date(packet.properties.userProperties.time).toLocaleString();
            } catch {
                var dateTime = new Date().toLocaleString();
            }
            $('#' + server + 'Log').append(`<tr class="${level_to_classname(level)}"><td style="white-space: nowrap;">${dateTime}</td><td align="center">${level_to_description(level)}</td><td>${message}</td></tr>`);
            
            if ($('#' + server + 'AutoScroll').is(':checked')) {
                var tableWrapper = document.getElementById(server + 'TableWrapper');
                tableWrapper.scrollTop = tableWrapper.scrollHeight;
            }
        }
        return;
    }

    // If we get here, the message wasn't expected
    console.log('Unhandled server->' + server + ' message: ' + JSON.stringify({server, subtopic, subtopic2, message}));
}

function level_to_classname(level) {
    switch (parseInt(level, 10)) {
        case 0: return 'danger';
        case 1: return 'danger';
        case 2: return 'danger';
        case 3: return 'danger';
        case 4: return 'warning';
        case 5: return 'info';
        case 6: return '';
        case 7: return 'success';
        default: return 'active';
    }
}

function level_to_description(level) {
    switch (parseInt(level, 10)) {
        case 0: return 'Emergency';
        case 1: return 'Alert';
        case 2: return 'Critical';
        case 3: return 'Error';
        case 4: return 'Warning';
        case 5: return 'Notice';
        case 6: return 'Normal';
        case 7: return 'Debug';
        default: return 'Unknown';
    }
}

function mqtt_message(topic, message, packet) {
    // Convert the message to a string and make it safe to display by replacing < with &lt;
    message = message.toString().replace(/</g, '&lt;');

    var arr = topic.split('/');
    if (arr[0] !== 'sbbs') {
        console.log('ERROR: topic does not start with "sbbs": ' + JSON.stringify({topic, message}));
        return;
    } else if (arr[1] !== system_qwk_id) {
        console.log('ERROR: topic does not start with BBSID (' + system_qwk_id + '): ' + JSON.stringify({topic, message}));
        return;
    }

    switch (arr[2]) {
        case undefined:
            // Ignore, message contains BBS Name
            break;

        case 'action':
            // Ignore, not handling anything under action at this time
            break;

        case 'host':
            // NB: arr[3] will be the host name
            switch (arr[4]) {
                case 'event':
                    switch (arr[5]) {
                        case undefined:
                            // Ignore, just says "thread started"
                            break;

                        case 'log':
                            var level = arr[6];
                            try {
                                var dateTime = timestamp_to_date(packet.properties.userProperties.time).toLocaleString();
                            } catch {
                                var dateTime = new Date().toLocaleString();
                            }
                            $('#eventsLog').append(`<tr class="${level_to_classname(level)}"><td style="white-space: nowrap;">${dateTime}</td><td align="center">${level_to_description(level)}</td><td>${message}</td></tr>`);
                            
                            var tab = document.getElementById('eventsTab');
                            tab.scrollTop = tab.scrollHeight;
                            break;

                        default:
                            console.log('ERROR: Unexpected topic element (5=' + arr[5] + '): ' + JSON.stringify({topic, message}));
                            break;
                    }
                    break;

                case 'server':
                    var server = arr[5];
                    var subtopic = arr[6];
                    var subtopic2 = arr[7];
                    handle_server_message(server, subtopic, subtopic2, message, packet);
                    break;

                default:
                    console.log('ERROR: Unexpected topic element (4=' + arr[4] + '): ' + JSON.stringify({topic, message}));
                    break;
            }
            break;

        case 'node':
            var node = arr[3];
            var messageType = arr[4];
            handle_node_message(node, messageType, message);
            break;

        default:
            console.log('ERROR: Unexpected topic element (2=' + arr[2] + '): ' + JSON.stringify({topic, message}));
            break;
    }
}

function timestamp_to_date(timestamp) {
    return new Date(timestamp.slice(0, 4), timestamp.slice(4, 6) - 1, timestamp.slice(6, 8), timestamp.slice(9, 11), timestamp.slice(11, 13), timestamp.slice(13, 15))
}

function update_clients() {
    $('#clientsTable').empty();
    
    // TODOY This displays clients in server order, might be nice to parse all the lists and sort by logonTime to show the longest-lived connections at the top
    var totalClients = 0;
    var servers = ['term', 'srvc', 'mail', 'ftp', 'web'];
    for (var i = 0; i < servers.length; i++) {
        if (client_lists[servers[i]]) {
            var lines = client_lists[servers[i]].split('\n');
            for (var j = 0; j < lines.length; j++) {
                // Examples: 
                //      20230813T134846-240	Telnet	1	Ree	::1	localhost	54400	13184
                //      20230813T151452-240	Telnet	0	<unknown user>	::1	localhost	59258	18256
                var arr = lines[j].split('\t');

                var logonTime = timestamp_to_date(arr[0]);
                var secondsElapsed = parseInt(((new Date()).getTime() - logonTime.getTime()) / 1000, 10);
                var hoursElapsed = parseInt(Math.floor(secondsElapsed / 3600), 10);
                secondsElapsed -= (hoursElapsed * 3600);
                var minutesElapsed = parseInt(Math.floor(secondsElapsed / 60), 10);
                secondsElapsed -= (minutesElapsed * 60);

                var protocol = arr[1];
                var username = arr[3];
                var address = arr[4];
                var hostname = arr[5];
                var port = arr[6];
                var socket = arr[7];

                $('#clientsTable').append(`<tr><td>${socket}</td><td>${protocol}</td><td>${username}</td><td>${address}</td><td>${hostname}</td><td>${port}</td><td>${hoursElapsed}h ${minutesElapsed}m ${secondsElapsed}s</td></tr>`);
                totalClients += 1;
            }
        }
    }

    $('#clientsCounterBadge').html(totalClients.toString());
}

// Add an onload handler to setup the node spy
window.addEventListener('load', (event) => {
    // Resize the panels to fit the window
    handle_resize();

    // Connect to mqtt broker and subscribe to a wildcard topic for this system
    var topics = ['sbbs/' + system_qwk_id + '/#'];
    mqtt_connect(topics, mqtt_message, console.log);

    // Set a 1 second timer to update the time display on the clients list (maybe for other purposes later too)
    setInterval(function() {
        update_clients();
    }, 1000);
    
    // Enable tooltips
    $('[data-toggle="tooltip"]').tooltip()
});

window.addEventListener('resize', (event) => {
    handle_resize();
});

$('.webmonitor-recycle').click(function() {
    var server = $(this).data('server');
    if (server) {
        mqtt_publish('sbbs/' + system_qwk_id + '/host/' + system_local_host_name + '/server/' + server + '/recycle', 'recycle');
    } else {
        mqtt_publish('sbbs/' + system_qwk_id + '/host/' + system_local_host_name + '/recycle', 'recycle');
    }
});

$('#ftpConfigureButton').click(function() {
    if (!webmonitor_configuration_enabled) {
        alert('Web Monitor Configuration is not enabled.\n\nSee the Documentation tab for more information.');
        return;
    }

    alert('FTP Server Configuration is not implemented yet');
});

$('#mailConfigureButton').click(function() {
    if (!webmonitor_configuration_enabled) {
        alert('Web Monitor Configuration is not enabled.\n\nSee the Documentation tab for more information.');
        return;
    }

    alert('Mail Server Configuration is not implemented yet');
});

$('#srvcConfigureButton').click(function() {
    if (!webmonitor_configuration_enabled) {
        alert('Web Monitor Configuration is not enabled.\n\nSee the Documentation tab for more information.');
        return;
    }

    alert('Services Configuration is not implemented yet');
});

$('#termConfigureButton').click(function() {
    if (!webmonitor_configuration_enabled) {
        alert('Web Monitor Configuration is not enabled.\n\nSee the Documentation tab for more information.');
        return;
    }

    $('#termModal').modal('show');

    $.ajax({
        url: './api/webmonitor/get-term.ssjs',
        type: 'GET',
        headers: { 'x-csrf-token': csrf_token }
    }).done(function (data) {
        if (data.error) {
            alert(data.error); // TODOX Display on form instead of alert
            $('#termModal').modal('hide');
        } else {
            $('#termModalLoading').hide();
            $('#termModalBody').show();
            $('#termSaveChanges').show();

            $('#termFirstNode').val(data.FirstNode);
            $('#termLastNode').val(data.LastNode);
            $('#termMaxConcurrentConnections').val(data.MaxConcurrentConnections);
            $('#termAutoStart').prop('checked', data.AutoStart);

            var options = data.Options;
            if (options) {
                options = options.split(/\s*[|]\s*/)
            } else {
                options = [];
            }

            $('#termXTRN_MINIMIZED').prop('checked', options.includes('XTRN_MINIMIZED'));
            $('#termYES_EVENTS').prop('checked', !options.includes('NO_EVENTS'));
            $('#termYES_QWK_EVENTS').prop('checked', !options.includes('NO_QWK_EVENTS'));
            $('#termYES_DOS').prop('checked', !options.includes('NO_DOS'));
            $('#termYES_HOST_LOOKUP').prop('checked', !options.includes('NO_HOST_LOOKUP'));
        }
    });
});

$('#termSaveChanges').click(function() {
    var requestModel = {
        FirstNode: $('#termFirstNode').val(),
        LastNode: $('#termLastNode').val(),
        MaxConcurrentConnections: $('#termMaxConcurrentConnections').val(),
        AutoStart: $('#termAutoStart').is(':checked'),
        XTRN_MINIMIZED: $('#termXTRN_MINIMIZED').is(':checked'),
        NO_EVENTS: !$('#termYES_EVENTS').is(':checked'),
        NO_QWK_EVENTS: !$('#termYES_QWK_EVENTS').is(':checked'),
        NO_DOS: !$('#termYES_DOS').is(':checked'),
        NO_HOST_LOOKUP: !$('#termYES_HOST_LOOKUP').is(':checked'),
    };

    $.ajax({
        url: './api/webmonitor/update-term.ssjs',
        type: 'POST',
        data: requestModel,
        headers: { 'x-csrf-token': csrf_token }
    }).done(function (data) {
        if (data.errors.length > 0) {
            var message = '';
            for (var i = 0; i < data.errors.length; i++) {
                message += data.errors[i].key + ' error: ' + data.errors[i].message + '\r\n';
            }
            alert(message); // TODOX Display on form instead of alert
        } else {
            $('#termModal').modal('hide');
        }
    });
});

$('#webConfigureButton').click(function() {
    if (!webmonitor_configuration_enabled) {
        alert('Web Monitor Configuration is not enabled.\n\nSee the Documentation tab for more information.');
        return;
    }

    alert('Web Server Configuration is not implemented yet');
});
