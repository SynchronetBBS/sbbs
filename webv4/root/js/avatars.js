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

const Avatars = new (function () {

    const gc = new GraphicsConverter('./images/cp437-ibm-vga8.png', 8, 16, 64, 4);

    function draw(data) {
        const img = new Image();
        img.addEventListener('load', () => document.querySelectorAll(`div[data-avatar="${data.user}"]:empty`).forEach(e => e.appendChild(img.cloneNode(true))));
        img.src = data.dataURL || 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFAAAABgCAYAAACKa/UAAAABiUlEQVR4Xu3c0YqCUBiFUXtKX1Gfcga6TPBgn6LDrG5zSy3377EQX8uy/ExeXwu8AH5t9w4CbH4Aox9AgFUg5p0Dnw44z3P8iC2+rmvbwSB9eQMBxuMHEOCugBGOBQEIcF/AKhwbAvC/Ad592TLyPruRpy8iAEeHcPA+QICHBIzwIa7txgABHhOwCh/z2mwNEGAUiHENBBgFYlwDAUaBGNdAgFEgxjUQYBSIcQ0EGAViXAMBRoEY10CAUSDGNRBgFIhxDQQYBWJcAwFGgRjXQIBRIMYf38DP73f33Vpng31+v9PvjQEYRwQgwEMCRvgQ13ZjgAD3BR6/Ct99mRILNFXgPMIA44N3AAJMZwEjHJ+pABBgmkCrcOObAD4e8K9fplwNPFxEAO4/+QjgoKKjn3oAAdazXPs3RwM1UAOvFYh7t4gAjAIxroEAo0CMayDAKBDjGggwCsS4BgKMAjGugQCjQIxrIMAoEOMaCDAKxLgGAowCMa6BAKNAjGsgwCgQ4xoIMArE+KiBv6PnKB+V8OyaAAAAAElFTkSuQmCC';
    }

    function cacheAvatar(data) {
        if (data.data) {
            gc.from_bin(atob(data.data), 10, 6, dataURL => {
                data.dataURL = dataURL;
                localStorage.setItem(`avatar-${data.user}`, JSON.stringify(data));
                draw(data);
            }, true);
        } else {
            data.data = null;
            localStorage.setItem(`avatar-${data.user}`, JSON.stringify(data));
            draw(data);
        }
    }

    function fromCache(user) {
        return JSON.parse(localStorage.getItem(`avatar-${user}`));
        // should return null if local copy is aged; as it is, avatars will remain in local storage until user clears it
    }

    async function fetchAvatar(user) {
        const u = [].concat(user);
        const data = await v4_get(`./api/system.ssjs?call=get-avatar&user=${u.join('&user=')}`);
        [].concat(data).forEach(cacheAvatar);
    }

    this.draw = async function (user) {
        const u = [].concat(user).filter(e => {
            const a = fromCache(e);
            if (a === null) return true; // Not cached
            if (a) draw(a);
        });
        if (u.length) fetchAvatar(u);
    }

})();