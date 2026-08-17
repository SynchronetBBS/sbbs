/* Where the WebSocket relay leaves what it knows about a connection.
 *
 * websocketservice.js writes a small file beside each connection it relays,
 * so the service on the far end can learn things a proxied socket cannot
 * tell it: the client's real address, and the user number of the web session
 * the client authenticated with, if any. Line 1 is the IP, line 2 the user
 * number, 0 for none.
 *
 * IT LIVES IN data_dir, NOT temp_dir. temp_dir is PER HOST, and a Synchronet
 * install can be shared by several of them -- so a relay on one machine and
 * a service on another never saw the same file, and the service was left to
 * conclude from its absence that the connection was direct and local. Where
 * that conclusion grants trust, being unable to see another host's sidecar
 * is the dangerous direction to fail in. data_dir is shared, so both ends
 * can reach it.
 *
 * THE NAME IS THE HANDSHAKE. There is nothing else passed between the two
 * ends, so both must derive the same name from the same connection, and the
 * name must be unique across every host that shares the install:
 *
 *     sbbs-ws-<address>-<port>.ip
 *
 * The relay uses the LOCAL address and port of the socket it opened to the
 * service. The service uses the REMOTE address and port of the connection
 * that arrived. Those are the same endpoint seen from the two ends, so they
 * agree without being told. A port alone is not enough: two hosts can hold
 * the same local port at the same moment, and in a shared directory that
 * turns a missing sidecar into a WRONG one, which is worse than none.
 *
 * The address is folded to a filename rather than used raw, because an IPv6
 * address carries colons -- illegal in a filename on Windows, which some
 * hosts sharing the install will be running -- and may carry a % scope. Any
 * character outside letters, digits and dots becomes a dash. The rule is
 * simple on purpose: it has to be reimplemented exactly in any service that
 * reads a sidecar without loading this file, including ones not written in
 * JavaScript.
 */

function ws_sidecar_name(ip, port)
{
	var safe = String(ip === undefined || ip === null ? '' : ip)
	           .replace(/[^A-Za-z0-9.]/g, '-');

	return 'sbbs-ws-' + safe + '-' + port + '.ip';
}

function ws_sidecar_path(ip, port)
{
	return system.data_dir + ws_sidecar_name(ip, port);
}
