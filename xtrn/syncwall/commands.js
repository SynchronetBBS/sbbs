var root = argv[0];

this.QUERY = function(client, packet) {

	var model = {
		LATEST : {
			permissions : [ 'SUBSCRIBE', 'UNSUBSCRIBE', 'READ', 'WRITE' ],
			types : [ 'object' ],
			validate : {
				object : function (obj) {
					if (typeof obj.x !== 'number' || obj.x < 0) return false;
					if (typeof obj.y !== 'number' || obj.y < 0) return false;
					if (typeof obj.u !== 'number' || obj.u < 0) return false;
					if (typeof obj.s !== 'number' || obj.s < 0) return false;
					if (typeof obj.c !== 'string' ||
						(obj.c.length > 1 && obj.c !== 'CLEAR')
					) {
						return false;
					}
					if (typeof obj.a !== 'number' || obj.a < 0) return false;
					return true;
				}
			}
		},
		SEQUENCE : { permissions : [ 'READ', 'SLICE' ] },
		STATE : { permissions : [ 'READ' ] },
		USERS : {
			permissions : [
				'SUBSCRIBE',
				'UNSUBSCRIBE',
				'READ',
				'PUSH',
				'SLICE'
			],
			types : [ 'string' ],
			validate : {
				string : function (str) {
					if (str.length < 1 || str.length > 25) return false;
					return true;
				}
			}
		},
		SYSTEMS : {
			permissions : [
				'SUBSCRIBE',
				'UNSUBSCRIBE',
				'READ',
				'PUSH',
				'SLICE'
			],
			types : [ 'string' ],
			validate : {
				string : function (str) {
					if (str.length < 1 || str.length > 50) return false;
					return true;
				}
			}
		},
		MONTHS : { permissions : [ 'READ', 'SLICE' ] }
	};

	function allowed(packet) {
		var location = packet.location.split('.')[0];
		if (typeof model[location] === 'undefined' ||
			typeof model[location].permissions === 'undefined'
		) {
			return false;
		}
		if (model[location].permissions.indexOf(packet.oper) >= 0 &&
			(	(	packet.oper !== 'WRITE' &&
					packet.oper !== 'SPLICE' &&
					packet.oper !== 'PUSH' &&
					packet.oper !== 'POP' &&
					packet.oper !== 'SHIFT'
				) ||
				(	typeof model[location].types === 'undefined' ||
					model[location].types.indexOf(typeof packet.data) >= 0
				)
			) &&
			(	typeof model[location].validate === 'undefined' ||
				typeof model[location].validate[typeof packet.data] !== 'function' ||
				model[location].validate[typeof packet.data](packet.data)
			)
		) {
			return true;
		}
		return false;
	}

	if (allowed(packet) || client.remote_ip_address === '127.0.0.1') {
		return false;
	} else {
		return true;
	}

}
