function Avatarizer() {

    const cache = { local : {}, network : {} };
    const graphics_converter = new GraphicsConverter('./images/cp437-ibm-vga8.png', 8, 16, 64, 4);

    function populate_image(target, image) {
        $("[name='avatar-" + target + "']").each(
            function () { $(this).append($(image).clone()); }
        );
    }

    this.get_localuser = function (usernumber, bin) {
        if (typeof cache.local[usernumber] == 'undefined') {
            cache.local[usernumber] = null;
            graphics_converter.from_bin(
                atob(bin), 10, 6, function (img) {
                    cache.local[usernumber] = img;
                    populate_image(usernumber, img);
                }
            );
        }
    }

    this.get_netuser = function (username, netaddr, bin) {
        if (typeof cache.network[netaddr] == 'undefined') {
            cache.network[netaddr] = {};
        }
        if (typeof cache.network[netaddr][username] == 'undefined') {
            cache.network[netaddr][username] = null;
            graphics_converter.from_bin(
                atob(bin), 10, 6, function (img) {
                    cache.network[netaddr][username] = img;
                    populate_image(username + '@' + netaddr, img);
                }
            );
        }
    }

}
