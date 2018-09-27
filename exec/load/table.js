load('sbbsdefs.js');

function column_sizes(data) {
  const sizes = data[0].map(function (e) {
    return {
      min : 0,
      max : 0
    };
  });
  data.forEach(function (e) {
    e.forEach(function (ee, i) {
      const max = strip_ctrl(ee).split(/\r\n/).reduce(function (a, c) {
        return c.length > a ? c.length : a;
      }, 0);
      if (max > sizes[i].max) sizes[i].max = max;
      const min = strip_ctrl(ee).split(' ').reduce(function (a, c) {
        return c.length > a ? c.length : a;
      }, 0);
      if (min > sizes[i].min) sizes[i].min = min;
    });
  });
  return sizes;
}

function table_size(sizes) {
  const tw = {
    min : sizes.reduce(function (a, c) { return a + c.min; }, 0),
    max : sizes.reduce(function (a, c) { return a + c.max; }, 0)
  };
  return tw;
}

/**
 * Turn a an Array of Arrays of Strings into a table for textmode display.
 * DokuWiki-style ::: rowspan and empty-cell colspan
 * To-do: rewrap colspans where original column got wrapped
 * @param {Array.<string[]>} data - [['the','heading','row'],['a','data','row']]
 * @param {string} line_color - CTRL-A code, default '\1n\1w'
 * @param {string} heading_color - CTRL-A code, default '\1h\1w'
 * @param {string} cell_color - CTRL-A code, default '\1n\1w'
 * @param {number} width - The terminal width (pass undefined for autodetect)
 * @returns {string}
 */
function table(data, line_color, heading_color, cell_color, width) {

  const framing = {
    ascii : {
      top_left : '+',
      top_right : '+',
      top_junction : '+',
      horizontal : '-',
      vertical : '|',
      junction : '+',
      left_junction : '+',
      right_junction : '+',
      bottom_left : '+',
      bottom_right : '+',
      bottom_junction : '+'
    },
    ex_ascii : {
      top_left : ascii(218),
      top_right : ascii(191),
      top_junction : ascii(194),
      horizontal : ascii(196),
      vertical : ascii(179),
      junction : ascii(197),
      left_junction : ascii(195),
      right_junction : ascii(180),
      bottom_left : ascii(192),
      bottom_right : ascii(217),
      bottom_junction : ascii(193)
    }
  };
  width = width || console.screen_columns - 1;
  line_color = line_color || '\1h\1w';
  heading_color = heading_color || '\1h\1w';
  cell_color = cell_color || '\1n\1w';

  const ldc = user.settings&USER_NO_EXASCII ? framing.ascii : framing.ex_ascii;

  const cs = column_sizes(data);
  const ts = table_size(cs);
  var widths; // Width math needs to be adjusted for border/separator characters
  var adj = (cs.length * 3) + 1;

  if (ts.min + adj > width) {
    // Should maybe just truncate lines, optionally
    throw 'Table too wide to be displayed';
  } else if (ts.max + adj > width) {
    // We can't display the table at its full size
    // We can display the table somewhere between its minimum and full size
    const w = width - (ts.min + adj);
    const d = ts.max - ts.min;
    // This needs some work to better fill the terminal width
    widths = cs.map(function (e) {
      return Math.ceil(e.min + (d / ((e.max - e.min) * w)));
    });

    // Something is quite wrong here.
    const tw = widths.reduce(function (a, c) { return a + c; }, 0) + adj;
    var wd = width - tw;
    for (var n = 0; n < wd; n++) {
      widths[n % widths.length]++;
    }

    var nr; // New rows
    var arr; // Wrapped cell
    for (var row = 0; row < data.length; row++) {
      nr = 0;
      for (var col = 0; col < data[row].length; col++) {
        arr = truncsp(word_wrap(data[row][col], widths[col])).split(/\n/);
        // If the cell has to wrap
        if (arr.length > 1) {
          // For each wrapped line in this cell
          for (var r = 0; r < arr.length; r++) {
            // If we haven't added a row for this wrapped line
            if (nr < r) {
              nr++;
              var add_row = [];
              add_row._stw = true;
              // For each column in the table, add a column to the new row
              for (var c = 0; c < data[row].length; c++) {
                // Default to rowspan
                add_row[c] = ' ';
              }
              // Add the new row to the table
              data.splice(row + r, 0, add_row);
            }
            // Add this line of wrapped text to the column in the added row
            data[row + r][col] = arr[r];
          }
        }
      }
    }
  } else {
    // We can display the table with every column at its max width
    widths = cs.map(function (e) { return e.max; });
  }

  var s;
  var line1;
  var line2;
  var lastrow;
  var lastcol;
  var colspan;
  var rowspan;
  var wrap; // Next row is a wrap
  var wrapping_colspan = [];
  const ret = [];
  for (var row = 0; row < data.length; row++) {
    lastrow = (row == data.length - 1);
    wrap = !lastrow && data[row + 1]._stw;
    if (!wrap && !data[row]._stw) wrapping_colspan = [];
    line1 = '\1+' + line_color + ldc.vertical + '\1-';
    line2 = '';
    for (var col = 0; col < widths.length; col++) {
      lastcol = (col == (widths.length - 1));
      colspan = (!lastcol && !data[row][col + 1]);
      if (!wrapping_colspan[col] && colspan && wrap) {
        wrapping_colspan[col] = true;
      }
      rowspan = (!lastrow && data[row + 1][col] == ':::');
      s = ' ' + (data[row][col] || '').replace(':::', '');
      while (strip_ctrl(s).length <= widths[col]) {
        s += ' ';
      }
      line1 += '\1+' + (row == 0 ? heading_color : cell_color) + s + ' ' + '\1-';
      // line1
      if (colspan || wrapping_colspan[col]) {
        line1 += ' ';
      } else {
        line1 += '\1+' + line_color + ldc.vertical + '\1-';
      }
      // line2
      if (!wrap) {
        if (rowspan) {
          if (col == 0) {
            line2 += ldc.vertical;
          } else if (data[row + 1][col - 1] == ':::') {
            line2 += ldc.left_junction;
          } else {
            line2 += ldc.right_junction;
          }
          for (var n = 0; n <= strip_ctrl(s).length; n++) {
            line2 += ' ';
          }
          if (lastcol) line2 += ldc.vertical;
        } else {
          if (col == 0) {
            if (lastrow) {
              line2 += ldc.bottom_left;
            } else {
              line2 += ldc.left_junction;
            }
          } else if (!lastrow && data[row + 1][col - 1] == ':::') {
            if (!data[row + 1][col]) {
              line2 += ldc.bottom_left;
            } else {
              line2 += ldc.left_junction;
            }
          } else {
            if (!data[row][col]) {
              if (lastrow || !data[row + 1][col]) {
                line2 += ldc.horizontal;
              } else {
                line2 += ldc.top_junction;
              }
            } else {
              if (lastrow || !data[row + 1][col]) {
                if (wrapping_colspan[col - 1]) {
                  line2 += ldc.horizontal;
                } else {
                  line2 += ldc.bottom_junction;
                }
              } else {
                line2 += ldc.junction;
              }
            }
          }
          for (var n = 0; n <= strip_ctrl(s).length; n++) {
            line2 += ldc.horizontal;
          }
          if (lastcol) {
            if (lastrow) {
              line2 += ldc.bottom_right;
            } else {
              line2 += ldc.right_junction;
            }
          }
        }
      }
    }
    ret.push(line1);
    if (!wrap) ret.push('\1+' + line_color + line2 + '\1-');
  }
  line1 = ldc.top_left;
  const hl = strip_ctrl(ret[0]);
  for (var n = 1; n < hl.length - 1; n++) {
    line1 += hl[n] == ldc.vertical ? ldc.top_junction : ldc.horizontal;
  }
  line1 += ldc.top_right;
  ret.unshift('\1+' + line_color + line1 + '\1-');
  return ret.join('\r\n') + '\r\n';

}
