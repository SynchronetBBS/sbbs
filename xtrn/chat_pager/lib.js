const PAGE_NEW = 0;
const PAGE_REPEAT = 1;
const PAGE_NONE = 2;

function load_settings(fp) {
    var ret = {};
    if (file_exists(fp)) {
        var f = new File(fp);
        if (f.open('r')) {
            var ini = f.iniGetAllObjects();
            f.close();
            ini.forEach(
                function (e) {
                    ret[e.name] = e;
                    delete ret[e.name].name;
                }
            )
        }
    }
    return ret;
}

function user_online(n) {
	return (
		system.node_list[n].status == 3 || system.node_list[n].status == 4
		? system.node_list[n].useron : 0
	);
}

// Returns array of 1-based node numbers that are currently paging the sysop
function nodes_paging() {
    return directory(
        system.ctrl_dir + 'syspage.*'
    ).map(
        function (e) {
            return parseInt(e.split('.').pop());
        }
    ).filter(
        function (e) {
            return (!isNaN(e) && e > 0 && e <= system.node_list.length);
        }
    );
}

var Scanner = function (settings) {

    var node_stat;

    this.reset = function () {
        node_stat = system.node_list.map(function () { return PAGE_NONE });
    }

    // Returns an array (index is zero-based node number) of PAGE_* status
    this.scan = function () {
        var np = nodes_paging();
        node_stat = node_stat.map(
            function (e, i) {
                if (e == PAGE_NONE && np.indexOf(i+1) >= 0) {
                    return PAGE_NEW;
                } else if (
                    (e == PAGE_NEW || e == PAGE_REPEAT) && np.indexOf(i+1) >= 0
                ) {
                    return PAGE_REPEAT;
                } else {
                    return PAGE_NONE;
                }
            }
        );
        return node_stat;
    }

    this.reset();

}
