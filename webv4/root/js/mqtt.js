var mqtt_client;

// Connect to the mqtt broker and subscribe to some topics    
function mqtt_connect(topics, message_callback, log_callback) {
    const options = {
        keepalive: 60,
        clientId: system_qwk_id + Math.random().toString(16).substr(2, 8),
        username: broker_username,
        password: broker_password,
        protocolId: 'MQTT',
        protocolVersion: 4,
        clean: true,
        reconnectPeriod: 5000,
        connectTimeout: 30 * 1000,
        will: {
            topic: 'WillMsg',
            payload: 'Connection Closed abnormally..!',
            qos: 0,
            retain: false
        },
    };

    // Build the host string to connect to based on whether ws:// or wss:// is required
    var protocol = location.protocol === 'https:' ? 'wss' : 'ws';
    var port = location.protocol === 'https:' ? broker_wss_port : broker_ws_port;
    const host = protocol + '://' + broker_addr + ':' + port + '/mqtt';

    // Set the debugging link to a modified version of the host string
    $('#mqtt-wss-reconnecting-link').attr('href', host.replace('wss://', 'https://'));

    // Connect to the mqtt broker
    log_callback('Connecting to mqtt broker (' + host + ')');
    mqtt_client = mqtt.connect(host, options);

    // Subscribe to topics on connect
    mqtt_client.on('connect', () => {
        log_callback('Connected');
        
        for (var i = 0; i < topics.length; i++) {
            mqtt_client.subscribe(topics[i], { qos: 2 });
        }

        $('#mqtt-ws-reconnecting-message').hide();
        $('#mqtt-wss-reconnecting-message').hide();
    });

    // Display an error when connection fails
    mqtt_client.on('error', (err) => {
        log_callback('Connection error: ', err);
        mqtt_client.end();
    });
    
    // Handle messages for subscribed topics
    mqtt_client.on('message', (topic, message, packet) => {
        message_callback(topic, message, packet);
    });
    
    // Display a message when the connection drops and the client is attempting to reconnect
    // Also show the debugging message if https is being used, because the sysop may need to resolve issues with their ssl cert
    mqtt_client.on('reconnect', () => {
        log_callback('Reconnecting...');
        
        if (location.protocol === 'https:') {
            $('#mqtt-wss-reconnecting-message').show();
        } else {
            $('#mqtt-ws-reconnecting-message').show();
        }
    });
}    

function mqtt_publish(topic, value) {
    mqtt_client.publish(topic, value, { qos: 1, retain: false });
}
