var root = argv[0];

this.QUERY = function(client, packet) {

	var model = {
		input : {
			_permissions : [ 'SUBSCRIBE', 'UNSUBSCRIBE', 'READ', 'WRITE' ],
			_types : [ 'object' ]
		},
		state : { 
			_permissions : [ 'SUBSCRIBE', 'UNSUBSCRIBE', 'READ', 'WHO' ]
		},
		users : {
			_permissions : [ 'SUBSCRIBE', 'UNSUBSCRIBE', 'READ' ],
			_types : [ 'string' ]
		},
		game : { _permissions : [ 'SUBSCRIBE', 'UNSUBSCRIBE', 'READ' ] },
		messages : { 
			_permissions : [ 'SUBSCRIBE', 'UNSUBSCRIBE', 'READ', 'SLICE' ]
		},
		news : {
			_permissions : [ 'SUBSCRIBE', 'UNSUBSCRIBE', 'READ', 'SLICE' ]
		}
	};

	function allowed(packet) {
		var location = packet.location.split('.')[0];
		if (typeof model[location] === 'undefined' ||
			typeof model[location]._permissions === 'undefined'
		) {
			return false;
		}
		if (model[location]._permissions.indexOf(packet.oper) >= 0 &&
			(	(	packet.oper !== 'WRITE' &&
					packet.oper !== 'SPLICE' &&
					packet.oper !== 'PUSH' &&
					packet.oper !== 'POP' &&
					packet.oper !== 'SHIFT'
				) ||
				(	typeof model[location]._types === 'undefined' ||
					model[location]._types.indexOf(typeof packet.data) >= 0
				)
			) &&
			(	typeof model[location]._validate === 'undefined' ||
				typeof model[location]._validate[typeof packet.data] !== 'function' ||
				model[location]._validate[typeof packet.data](packet.data)
			)
		) {
			return true;
		}
		return false;
	}

	if (allowed(packet) || client.remote_ip_address === '127.0.0.1') return false;

	return true;

}
