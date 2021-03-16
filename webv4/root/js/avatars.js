const Avatars = new (function () {

    function draw(data) {
        const img = new Image();
        img.addEventListener('load', () => {
            document.querySelectorAll(`div[data-avatar="${data.user}"]:empty`).forEach(e => {
                e.appendChild(img.cloneNode(true))
            });
        });
        img.src = data.dataURL || 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFAAAABgCAYAAACKa/UAAAABiUlEQVR4Xu3c0YqCUBiFUXtKX1Gfcga6TPBgn6LDrG5zSy3377EQX8uy/ExeXwu8AH5t9w4CbH4Aox9AgFUg5p0Dnw44z3P8iC2+rmvbwSB9eQMBxuMHEOCugBGOBQEIcF/AKhwbAvC/Ad592TLyPruRpy8iAEeHcPA+QICHBIzwIa7txgABHhOwCh/z2mwNEGAUiHENBBgFYlwDAUaBGNdAgFEgxjUQYBSIcQ0EGAViXAMBRoEY10CAUSDGNRBgFIhxDQQYBWJcAwFGgRjXQIBRIMYf38DP73f33Vpng31+v9PvjQEYRwQgwEMCRvgQ13ZjgAD3BR6/Ct99mRILNFXgPMIA44N3AAJMZwEjHJ+pABBgmkCrcOObAD4e8K9fplwNPFxEAO4/+QjgoKKjn3oAAdazXPs3RwM1UAOvFYh7t4gAjAIxroEAo0CMayDAKBDjGggwCsS4BgKMAjGugQCjQIxrIMAoEOMaCDAKxLgGAowCMa6BAKNAjGsgwCgQ4xoIMArE+KiBv6PnKB+V8OyaAAAAAElFTkSuQmCC';
    }

    this.draw = async function (user) {

        const u = [];
        for (let e of [].concat(user)) {
            const a = await sbbs.avatars.users.get(e);
            if (!a) {
                u.push(e);
            } else {
                draw(a);
            }
        }
        if (!u.length) return;

        const a = await v4_get(`./api/system.ssjs?call=get-avatar&user=${u.join('&user=')}`);
        a.forEach(async e => {
            if (e.data) {
                const dataURL = await Graphics.binToPNG(atob(e.data).split('').map(e => e.charCodeAt(0)), 10, true);
                const o = { ...e, dataURL };
                sbbs.avatars.users.set(o);
                draw(o);
            } else {
                const o = { user: e.user, data: null, dataURL: null, created: -1, updated: -1 };
                sbbs.avatars.users.set(o);
                draw(o);
            }
        });

    }

})();