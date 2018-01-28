const avatar_lib = load({}, 'avatar_lib.js');

function Avatars() {

    const cache = { local : {}, network : {} };

    this.__defineGetter__('cache', function () { return cache; });

    this.get_localuser = function (usernumber) {
        if (typeof cache.local[usernumber] == 'undefined') {
            var data = avatar_lib.read_localuser(usernumber);
            if (!data || data.disabled) {
                cache.local[usernumber] = false;
            } else {
                cache.local[usernumber] = data.data;
            }
        } else {
            return cache.local[usernumber];
        }
    }

    this.get_netuser = function (username, netaddr) {
        if (typeof cache.network[netaddr] == 'undefined') {
            cache.network[netaddr] = {};
        }
        if (typeof cache.network[netaddr][username] == 'undefined') {
            var data = avatar_lib.read_netuser(username, netaddr);
            if (!data || data.disabled) {
                cache.network[netaddr][username] = false;
            } else {
                cache.network[netaddr][username] = data.data;
            }
        } else {
            return cache.network[netaddr][username];
        }
    }

}
