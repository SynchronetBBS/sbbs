require('http.js', 'HTTPRequest');

function get_splash() {
    var DefaultSplash = 'G1swbQ0KG1syOEMbWzE7MzBtICAgG1swbRtbMTlDG1szNG3cG1sxbdwbWzBtICAgG1szNG3cDQobWzM3bRtbMTNDG1sxbSAbWzMzbSAbWzBtG1sxM0MbWzM0bdwbWzM3bSAgG1szNG3c3LAbWzQ0OzMwbbIbWzQwOzM3bRtbMTFDG1szNG0gIBtbMW3c3xtbMG0bWzlDG1sxOzMwbWdqL9rkDQobWzBtG1s2QxtbMW1zeW5jaHJvbmV0G1swbSAgIBtbMTszMG3c3BtbMG0gG1sxOzMwbd8bWzBtICAgG1szNG3+G1szN20bWzVDG1szNG0gIN+y29zcG1szN20gICAbWzM0bdzcICAgG1sxbd4bWzA7MzRt3Q0KG1szN20bWzdDG1szNG3cG1szN20gG1szNG3+G1szN20gICAg3NzcG1sxbbGxsBtbMG0gICAgG1szNG3c3Nzc2xtbMTs0NG3cICD+G1swOzM0bdvc3tsbWzQ0OzMwbbAbWzQwOzM0bbLfIN4bWzE7NDRtsLAbWzA7MzRtsbDcG1sxbdwbWzQ2bbIbWzA7MzRtIBtbMzdt3xtbMTszMG3fG1swbdsbWzE7NDdt3Nzc3NwbWzBtsiAgIBtbMzRt3xtbMzdtICAbWzM0bd/f3NwNChtbMzdtICAgIBtbMTszNG3cG1swOzM0bd8bWzM3bSAgINwbWzFt3BtbNDdt3BtbNDBt29sbWzQ3bdvb27EbWzQwbbAbWzBtICAbWzM0bdwbWzE7NDRt3NwbWzQwbdvf3xtbMDszNG3fG1szN20g3NzcIBtbMzRt3BtbMTs0NG2wG1swOzM0bdvfG1szN20g3NzcIBtbMzRt3htbMTs0NG2yG1swOzM0bdwbWzQ0OzMybbAbWzQ2OzM0bbIbWzE7NDBt398bWzA7MzRt3xtbMzZtIBtbMzdtICAbWzE7NDc7MzBt3BtbMzdtstvb2xtbNDBtsRtbMG0gINzc3Nzc3CAbWzE7MzRtsN0NChtbMG0gIBtbMTszNG0g3htbMDszNG3dG1szN20gICAbWzFtsBtbNDdtstvb29/f29uyG1swbd0gG1sxOzM0bbEbWzQ2bbIbWzQ3bbIbWzQ2bbIbWzQwbbIbWzBtIBtbMTs0N23e29vbG1s0MG2xG1swbSAbWzE7MzRt3BtbNDRtshtbMDszNG3fG1szN23cG1sxOzQ3bbCx29uyG1swbd0gG1s0NjszNG3cG1sxOzQwbd8bWzBtIBtbMTszMG3cG1swbdwbWzE7NDdtsLCwsLAbWzBt3NsbWzE7NDdtstvbshtbNDBtsBtbMG0g2xtbMTs0N23b27AbWzBtIBtbMzRt3N8bWzFt39zcG1swOzM0bdwNChtbMzdtICAgIBtbNDY7MzBtshtbNDA7MzRtICAbWzE7MzBt3htbNDc7MzdtsbLb29wbWzBt3CDfG1sxOzQ3bd/fG1swbdvcIBtbMTszNG3f3xtbMDszNG0gG1sxbbEbWzBtIBtbMTs0N23e29sbWzQwbbIbWzBt3RtbMTszNG3eG1s0Nm2yG1swbSAbWzE7MzBt3BtbMG2y3xtbMTs0N22xstvbG1s0MG2xG1swbSAg3BtbMTs0N22wsbKysrKysrEbWzBtIBtbMTs0NzszMG2wG1szN23b29sbWzQwbbEbWzBtIN4bWzE7NDdt3tuyG1swbd0bWzM0bd8bWzE7NDRtsBtbNDBt2xtbNDdtsrIbWzQwbbEbWzA7MzRtsrEbWzM3bSAbWzM0bbANCiAgG1sxbSAbWzA7MzRt3NwbWzE7NDRtsBtbMDszNG3cG1szN20gG1sxOzMwbd8bWzBt3xtbMTs0N23f39vb3BtbMG3cICAbWzE7MzBt3BtbMG3c3NwbWzFtsBtbMzBt3RtbMzRtsBtbMG0gG1sxOzQ3bd7b2xtbNDBtsbAbWzBt3BtbMzZt3xtbMzdtICAbWzM0bdwbWzM3bSCwG1sxOzQ3bbLbsrAbWzBtIBtbMW2wG1s0N22xstvb39vb2xtbNDBtshtbMG3dIBtbMTszMG3eG1s0NzszN22x29uxG1swbdzc2xtbMTs0N23b2xtbNDBtstzcG1swbdwbWzFt3BtbMG3c3CDcIBtbMTszNG2wG1swOzQ0OzMwbbIbWzBtDQobWzM0bSAg2xtbMTs0NG2wshtbMDszNG0gG1sxOzQ0bbEbWzQwbbEbWzA7NDY7MzRtshtbNDBt3NwbWzM3bSDf3xtbMTs0N23f39wbWzBt3CAbWzE7NDdtsdvb2xtbMG3dIBtbMTszMG3eG1s0NzszN23e29sbWzQwbbCyG1s0N23b29wbWzQwbdwbWzBt3BtbMzRtIBtbMTszN20gG1s0N22ysrEgG1s0MDszMG3eG1s0NzszN22xstvbG1swbd8gG1sxbbAbWzQ3bdvb3RtbMG0gG1szNG3+IBtbMTszN22wG1s0N22y29vb29vb27KxG1swbSAbWzM0bdwbWzFt3BtbMDszNm3cG1sxOzM0bdwbWzA7MzRt3NwbWzFt3BtbMDszNG3fDQog3htbNDQ7MzZtsBtbNDY7MzRtshtbMTs0MG3d3t/fG1swbSDc3NwgG1szNG3cG1szN20g3tsbWzE7NDdt3BtbMG3bG1sxbbEbWzQ3bbLb29wbWzBt3N4bWzE7NDdt29uyG1swbd0bWzE7NDdtsbLb29vb3BtbMG3cG1sxOzQ3bbKxsBtbMG3bG1sxOzMwbd4bWzQ3OzM3bbCy27EbWzBtIBtbMzRtINwbWzE7MzBt3xtbMG3fG1sxbd/fG1szMG0gG1swbSDeG1sxOzQ3bbCysRtbMG3dICDbG1sxOzQ3bbKyshtbMG3dIBtbMzRt3xtbMW3f3BtbNDZtshtbMG0NChtbMzRtICDfG1sxbdsbWzQ2bd+yG1s0MG3dG1swbbAbWzQ3OzMwbbAbWzQwOzM3bdsbWzFtsRtbMG3c3NzcG1sxOzQ3bSAbWzQwbbGxG1s0N20gG1swbdwbWzE7NDc7MzBt3BtbNDA7Mzdt3xtbNDdt39/b29vbsRtbMG0gG1sxOzQ3bbCxsbAbWzBtIBtbMW3f2xtbNDdt29uysBtbMG3bIBtbMTs0NzszMG3cG1szN22wsbAbWzBt29wgG1szNG3f3xtbMzdtINyxG1sxOzMwbd0bWzBtINsbWzE7NDdtsbEbWzBt3RtbMzRt3twbWzM3bd4bWzE7NDdtsbGxG1swbdsgG1sxOzQ0OzM2bbAbWzQwOzM0bd/fDQobWzBtG1s1QxtbMTszNG0gG1swOzM0bSAbWzM3bbAbWzQ3OzMwbbAbWzQwOzM3bdvb2xtbMTs0N23bsrGwsCAbWzBt29vbG1sxOzQ3OzMwbd8bWzQwbdwbWzBtIN4bWzE7NDdtsrKyG1swbd3e2xtbMTs0N22wsBtbMG3b3SAgG1sxbd/bG1s0N23bsbAbWzBt3SDfG1sxOzQ3bSAbWzQwbbGxG1s0N20g3BtbMG3bG1sxOzQ3bf4bWzBt29uyG1sxbd0bWzA7MzRt3BtbMzdt3htbMTs0N22wG1swbdsbWzFt3RtbMzRt3htbMDs0NDszNm2wG1s0MDszN23eG1sxOzQ3bbCwsBtbMG3bG1sxbd0bWzA7MzZt3t0NChtbMzdtICAgIBtbMzZtsBtbNDY7MzBtshtbNDA7MzdtICAbWzFt3xtbNDdt3NzcG1s0MG2yG1swbd3f39/fIBtbMTszNG3c3BtbMDszNG3c3BtbMzdtINsbWzE7NDdtsLCwG1swbSAbWzFt39/fG1s0N23c3NwbWzQwbd0bWzA7MzRt3xtbMW3cG1swbSAbWzFt3xtbNDdtsrEbWzQwbbEbWzBt3CAbWzM0bdwbWzM3bSDf39vbG1s0NzszMG2wG1s0MDszN22yG1sxbd/fG1swbSAg3tsbWzFt3xtbMDszNG0gG1s0NDszNm2yG1s0MDszN20gG1sxbbAbWzBt29sbWzQ3OzMwbbAbWzE7NDA7MzdtsRtbMG0gG1sxOzM2bd8NChtbMG0bWzZDG1szNm3fG1szN20gIBtbMzZt3BtbMzdtICAbWzM2bdzcG1sxbdwbWzA7MzZt39/bG1sxbbEbWzQ0bbAbWzA7NDQ7MzZtsBtbNDA7MzRtst8bWzM3bd4bWzE7NDc7MzNtIBtbMG3bG1sxOzQ3OzMzbbAbWzQwbd0bWzMwbbAbWzBtsBtbNDc7MzBtshtbNDA7MzdtICAbWzM0bdwbWzM3bSAbWzFt3xtbMG0gG1sxOzM2bbAbWzQ2OzM0bd8bWzQwbdzcG1swbSAbWzFt39/f3NwbWzBtIBtbMzRt3yDc3NwgG1szN20gG1szNG0gG1sxOzM3bdwbWzBt3xtbMW3fG1swbSAbWzE7MzRt3htbMG0g39/fG1sxbd/fDQobWzBtG1sxM0MbWzM0bSAbWzM3bSAgG1szNG2wG1szN20gG1szNG2wG1s0NDszMG2yG1s0MDszN20gIBtbMW3cG1s0N23cG1szM23c27LbG1s0MDszN23b29sbWzMzbd8bWzBtIBtbMzRtICDf3xtbNDQ7MzZtIN/fsBtbNDA7MzRt2xtbMW3f3BtbMG0bWzVD/iAgICDfG1s1QxtbMzRtIP4NChtbMzdtG1syNkMbWzE7MzNt3Nvb3xtbMG0bWzEyQxtbMzRtIN4bWzFt3bAbWzA7NDQ7MzBtshtbNDA7MzdtIBtbMTszMG0gIBtbMzNtYmJzG1swbSBzb2Z0d2FyZQ0KG1syNUMbWzE7MzNt3NsbWzM3bd8bWzBtG1sxMkMbWzM0bdwbWzM3bSAgG1szNG3fDQobWzM3bRtbMjRDG1sxOzM2bdwbWzM3bdsbWzMzbdzcDQobWzBtG1syNUMbWzE7MzNt3N8NChtbMG0bWzI1QxtbMTszM23dDQo=';
    if (settings && settings.ftelnet_splash) {
        var f = new File(settings.ftelnet_splash);
        f.open('rb');
        var splash = base64_encode(f.read());
        f.close();
        return splash || DefaultSplash;
    } else {
        return DefaultSplash;
    }
}

function get_cached_url(f) {
    if (f.open('r')) {
        const ret = f.read();
        f.close();
        return ret;
    }
    return 'http://embed-v2.ftelnet.ca/js/ftelnet-loader.norip.noxfer.js?v=2018-09-14';
}

function get_url() {

    const f = new File(system.temp_dir + 'ftelnet.url');
    if (!f.exists || time() - f.date > 86400) {
        try {
            var str = (new HTTPRequest()).Get('http://embed-v2.ftelnet.ca/js/ftelnet-loader.norip.noxfer.js?v=' + (new Date()).getTime());
        } catch (err) {
            log(LOG_ERR, 'Failed to fetch fTelnet URL');
            str = '';
        }
        const match = str.match(/^.+\ssrc="(.*)"\s.+/);
        if (match !== null) {
            if (f.open('w')) {
                f.write(match[1]);
                f.close();
            }
            return match[1];
        }
    }
    return get_cached_url(f);
}

this;
