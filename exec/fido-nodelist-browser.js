require('frame.js', 'Frame');
require('tree.js', 'Tree');
require('scrollbar.js', 'ScrollBar');
require('typeahead.js', 'Typeahead');
require('fido_syscfg.js', 'FTNDomains');
require('ftn_nodelist.js', 'NodeList');

function node_info_popup(node, frame, settings) {
  const fmt = "%10s: \1+\1h\1c%s\1-\r\n";
  const popup = new Frame(
    frame.x + 1,
    Math.floor((frame.y + frame.height - 16) / 2),
    frame.width - 2,
    16,
    BG_BLUE|WHITE,
    frame
  );
  popup.putmsg('\r\n Node Details');
  popup.gotoxy(1, 4);
  popup.putmsg(format(fmt, 'Address', node.addr));
  popup.putmsg(format(fmt, 'Name', node.name));
  popup.putmsg(format(fmt, 'Sysop', node.sysop));
  popup.putmsg(format(fmt, 'Location', node.location));
  popup.putmsg(format(fmt, 'Hub', node.hub));
  if (node.flags.INA || node.flags.IP) {
    popup.putmsg(format(fmt, 'Internet', node.flags.INA || node.flags.IP));
  }
  const protocol = [];
  if (node.flags.IBN) protocol.push('Binkp');
  if (node.flags.IFC) protocol.push('ifcico');
  if (node.flags.ITN) protocol.push('Telnet');
  if (node.flags.IVM) protocol.push('Vmodem');
  if (protocol.length) {
    popup.putmsg(format(fmt, 'Protocol', protocol.join(', ')));
  }
  protocol = undefined;
  const email = [];
  if (node.flags.IEM) email.push(node.flags.IEM);
  if (node.flags.ITX) email.push('TransX');
  if (node.flags.IUC) email.push('UUEncode');
  if (node.flags.IMI) email.push('MIME');
  if (node.flags.ISE) email.push('SEAT');
  if (email.length) popup.putmsg(format(fmt, 'Email', email.join(', ')));
  email = undefined;
  if (node.flags.PING) popup.putmsg('Accepts PING\r\n');
  const other = [
    'ZEC', 'REC', 'NEC', 'NC', 'SDS', 'SMH', 'RPK', 'NPK', 'ENC', 'CDP'
  ].filter(function (e) {
    return typeof node.flags[e] != 'undefined';
  });
  if (other.length) popup.putmsg(format(fmt, 'Flags', other.join(', ')));
  popup.gotoxy(1, 15);
  popup.putmsg(' \1h\1cS\1h\1wend netmail  \1h\1cC\1h\1wlose');
  other = undefined;
  popup.open();
  frame.cycle();
  console.gotoxy(console.screen_columns, console.screen_rows);
  var user_input;
  while((user_input = console.inkey(K_NONE).toLowerCase()) == '') {
    yield();
  }
  popup.close();
  if (user_input == 's') {
    console.clear(LIGHTGRAY);
    bbs.netmail(node.sysop + '@' + node.addr, WM_NETMAIL);
    console.clear(LIGHTGRAY);
    settings.frame.invalidate();
  }
}

function populate_node_tree(filename, zone, net, tree, settings) {
  const nodelist = new NodeList(filename);
  nodelist.entries.forEach(function (e) {
    const z = e.addr.split(':')[0];
    if (z != zone) return;
    const n = e.addr.match(/\d+:(\d+)\//)[1];
    if (n != net) return;
    const w = Math.floor(((tree.frame.width - 15 - 3) / 2) - 1);
    tree.addItem(format(
      '%-15s%-' + w + 's%-' + w + 's',
      e.addr, e.name.substr(0, w), e.location.substr(0, w)
    ), node_info_popup, e, tree.frame, settings);
  });
  // Not sure if it's worth sorting this tree
  // Nodes probably could be out of order but they generally appear to be in
  // sequence by the time we get here.
}

function populate_net_tree(filename, zone, tree, settings) {
  const nodelist = new NodeList(filename);
  const nets = [];
  nodelist.entries.forEach(function (e) {
    const zn = e.addr.match(/(\d+):(\d+)\//);
    if (zn[1] != zone) return;
    if (nets.indexOf(zn[2]) > -1) return;
    nets.push(zn[2]);
    const net_tree = tree.addTree('Net ' + zn[2]);
    const idx = tree.items.length - 1;
    net_tree.onOpen = function () {
      populate_node_tree(filename, zone, zn[2], net_tree, settings);
      if (settings.auto_close_net) {
        tree.items.forEach(function (e) {
          if (e.text != net_tree.text) {
            e.close();
            e.index = -1;
          }
        });
      }
      tree.refresh();
    }
    net_tree.onClose = function () {
      net_tree.items = [];
    }
  });
  tree.items.sort(function (a, b) {
    if (parseInt(a.text.split(' ')[1]) < parseInt(b.text.split(' ')[1])) {
      return -1;
    } else {
      return 1;
    }
  });
}

function populate_zone_tree(filename, tree, settings) {
  const nodelist = new NodeList(filename);
  const zones = [];
  nodelist.entries.forEach(function (e) {
    const zone = e.addr.split(':')[0];
    if (zones.indexOf(zone) > -1) return;
    zones.push(zone);
    const zone_tree = tree.addTree('Zone ' + zone);
    const idx = tree.items.length - 1;
    zone_tree.onOpen = function () {
      populate_net_tree(filename, zone, zone_tree, settings);
      if (settings.auto_close_zone) {
        tree.items.forEach(function (e) {
          if (e.text != zone_tree.text) {
            e.close();
            e.index = -1;
          }
        });
      }
      tree.refresh();
    }
    zone_tree.onClose = function () {
      zone_tree.items = [];
    }
  });
  tree.items.sort(function (a, b) {
    if (parseInt(a.text.split(' ')[1]) < parseInt(b.text.split(' ')[1])) {
      return -1;
    } else {
      return 1;
    }
  });
}

function populate_domain_tree(filename, domain, tree, settings) {
  try {
    const nodelist = new NodeList(filename);
  } catch (err) {
    log(LOG_ERR, format('Error parsing %s: %s', filename, err));
    return;
  }
  const domain_tree = tree.addTree(domain);
  const idx = tree.items.length - 1;
  domain_tree.onOpen = function () {
    populate_zone_tree(filename, domain_tree, settings);
    if (settings.auto_close_domain) {
      tree.items.forEach(function (e) {
        if (e.text != domain_tree.text) {
          e.close();
          e.index = -1;
        }
      });
    }
    tree.refresh();
  }
  domain_tree.onClose = function () {
    domain_tree.items = [];
  }
}

function populate_tree(tree, settings) {
  const ftn_domains = new FTNDomains();
  Object.keys(ftn_domains.nodeListFN).forEach(function (e) {
    if (!file_exists(ftn_domains.nodeListFN[e])) return;
    const dn = settings['domain_' + e] || e;
    populate_domain_tree(ftn_domains.nodeListFN[e], dn, tree, settings);
  });
  Object.keys(settings).forEach(function (e) {
    if (e.search(/^nodelist_/) > -1 && file_exists(settings[e])) {
      const dn = e.replace(/^nodelist_/, '');
      populate_domain_tree(settings[e], dn, tree, settings);
    }
  });
  tree.items.sort(function (a, b) {
    return a.text.toLowerCase() < b.text.toLowerCase() ? -1 : 1;
  });
  tree.refresh();
}

function search(frame, settings) {

  const sframe = new Frame(frame.x, frame.y + 4, frame.width, 3, BG_BLUE|WHITE, frame);
  sframe.putmsg('Search by address, system name, sysop, or location:');
  sframe.gotoxy(1, 3);
  sframe.putmsg("Type a query and wait for results, don't hit enter.");
  sframe.open();

  const typeahead = new Typeahead({
    x : 1,
    y : sframe.y + 1,
    bg : 0,
    prompt: '',
    frame: sframe,
    datasources: [
      function (str) {
        const w = (Math.floor((frame.width - 9 - 15) / 3) - 1);
        const ret = [];
        const ftn_domains = new FTNDomains();
        const files = Object.keys(ftn_domains.nodeListFN).map(function (e) {
          return ftn_domains.nodeListFN[e];
        });
        Object.keys(settings).forEach(function (e) {
          if (e.search(/^nodelist_/) > -1) files.push(settings[e]);
        });
        files.forEach(function (e) {
          if (!file_exists(e)) return
          const nodelist = new NodeList(e);
          nodelist.entries.forEach(function (e) {
            var r = ['addr', 'sysop', 'name', 'location'].some(function (ee) {
              if (e[ee].search(new RegExp(str, 'i')) > -1) {
                ret.push({
                  node : e,
                  text : format(
                    '%-15s%-' + w + 's %-' + w + 's %-' + w + 's',
                    e.addr,
                    e.name.substr(0, w),
                    e.location.substr(0, w),
                    e.sysop.substr(0, w)
                  )
                });
                return true;
              }
              return false;
            });
          });
        });
        return ret;
      }
    ]
  });

  var user_input;
  while (typeof user_input == 'boolean' || typeof user_input == 'undefined') {
    user_input = typeahead.inkey(console.inkey(K_NONE));
    typeahead.cycle();
    if (frame.cycle()) {
      console.gotoxy(console.screen_columns, console.screen_rows);
      typeahead.updateCursor();
    }
    yield();
  }
  typeahead.close();
  sframe.close();

  if (typeof user_input == 'object') {
    node_info_popup(user_input.node, frame, settings);
  }

}

function main() {

  const ca = console.attributes;
  const bss = bbs.sys_status;
  bbs.sys_status|=SS_MOFF;

  const frame = new Frame(
    1, 1, console.screen_columns, console.screen_rows, BG_BLUE|WHITE
  );
  const tree_frame = new Frame(
    frame.x, frame.y + 2, frame.width, frame.height - 4, BG_BLACK, frame
  );
  const tree = new Tree(tree_frame);
  tree.colors.lfg = WHITE;
  tree.colors.lbg = BG_CYAN;
  tree.colors.tfg = LIGHTCYAN;
  tree.colors.kfg = WHITE;
  tree.colors.xfg = LIGHTCYAN;
  const scrollbar = new ScrollBar(tree, {autohide : true});

  // Valid settings:
  // auto_close_domain t/f
  // auto_close_zone t/f
  // auto_close_net t/f
  // nodelist_xxx /path/to/some/nodelist
  const defaults = {
    auto_close_domain: true,
    auto_close_zone: true,
    auto_close_net: true
  };
  const settings = load({}, 'modopts.js', 'fido_nodelist_browser') || {};
  Object.keys(defaults).forEach(function (e) {
    if (typeof settings[e] == 'undefined') settings[e] = defaults[e];
  });
  settings.frame = frame;

  frame.putmsg('Nodelist Browser');
  frame.gotoxy(1, frame.height);
  frame.putmsg('\1h\1cS\1h\1wearch  \1h\1cQ\1h\1wuit');
  populate_tree(tree, settings);

  console.clear(LIGHTGRAY);
  frame.open();
  tree.open();
  frame.cycle();
  scrollbar.cycle();

  var user_input;
  while ((user_input = console.getkey(K_NONE).toLowerCase()) != 'q') {
    if (user_input == 's') {
      search(frame, settings);
    } else {
      tree.getcmd(user_input);
      scrollbar.cycle();
    }
    if (frame.cycle()) {
      console.gotoxy(console.screen_columns, console.screen_rows);
    }
    yield();
  }

  frame.close();
  bbs.sys_status = bss;
  console.clear(ca);

}

main();
