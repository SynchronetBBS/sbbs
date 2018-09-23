/*
 * Synchronet-flavoured Markdown
 * # Heading
 * Lists end on first line not starting with a list prefix
 * * Unordered list item\r\n
 *  * Unordered list sub-item\r\n
 *   * Unordered list sub-sub-item\r\n ...
 * 1. Numbered list item\r\n ...
 *  1. Numbered list sub-item\r\n ...
 *  2. Another numbered list sub-item\r\n ...
 *   1. Numbered list sub-sub-item\r\n ...
 * 2. Another numbered list item
  * *Emphasis* (high colour)
 * _ctext_ colour (where c is a CTRL-A colour)
 * > blockquote
 * [Link text](url) a link
 * ![Alt text](url) an image
 * --- A horizontal rule
 * this|is|a|table|header
 * this|is|a|table|row
 */

 /* To do:
  * configurable list indentation
  * configurable heading underline
  * configurable heading attribute
  * configurable link attribute
  * configurable image attribute
  * configurable class names to add to:
  * <ul> <ol> <li> <table> <th> <tr> <td> <img> <a>
  * configurable tags instead of <b> and <i>
 */

load('sbbsdefs.js');

if (typeof Frame == 'undefined') Frame = false;

function Markdown(target) {
  const state = {
    list_level : 0,
    links : [],
    images : [],
    table : [],
    blockquote : false,
    list_stack : []
  }
  if (Frame && target instanceof Frame) target.word_wrap = true;
  Object.defineProperty(this, 'state', { get : function () {
    return state;
  }});
  Object.defineProperty(this, 'target', {
    get : function () {
      return target;
    },
    set : function (t) {
      if (t == 'html') {
        target = t;
      } else if (Frame && t instanceof Frame) {
        target = t;
      } else if (
        typeof t.screen_columns == 'number' && typeof t.putmsg == 'function'
      ) {
        target = t;
      } else {
        throw 'Invalid output target';
      }
    }
  });
  Object.defineProperty(this, 'columns', { get : function () {
    if (target == 'html') {
      return 0;
    } else if (Frame && target instanceof Frame) {
      return target.width;
    } else {
      return target.screen_columns;
    }
  }});
}

Markdown.prototype.colorize_console = function (str) {
  const self = this;
  str = str.replace(/_([krgybmcw0-7])/ig, function (m, c1, c2) {
    return '\1+\1' + c1;
  });
  return str.replace(/_/g, '\1-');
}

Markdown.prototype.render_text_console = function (text) {
  log('rtc ' + text);
  const self = this;
  var ret = text.replace(/\*([^\*]+)\*/g, function (m, c) {
    return '\1+\1h' + c + '\1-';
  });
  ret = ret.replace(/!\[([^\]]+)\]\(([^\)]+)\)/g, function (m, c1, c2) {
    self.state.images.push({ text : c1, link : c2 });
    return '\1+\1m' + c1 + ' [' + self.state.images.length + ']\1-';
  });
  ret = ret.replace(/\[([^\]]+)\]\(([^\)]+)\)/g, function (m, c1, c2) {
    self.state.links.push({ text : c1, link : c2 });
    return '\1+\1c' + c1 + ' [' + self.state.links.length + ']\1-';
  });
  return this.colorize_console(ret);
}

Markdown.prototype.italicize = function (str) {
  const self = this;
  str = str.replace(/_([krgybmcw0-7])(.+)/ig, function (m, c1, c2) {
    return '<i>' + self.italicize(c2);
  });
  return str.replace(/_/g, '</i>');
}

Markdown.prototype.render_text_html = function (text) {
  const self = this;
  var ret = text.replace(/\*([^\*]+)\*/g, function (m, c) {
    return '<b>' + c + '</b>';
  });
  ret = ret.replace(/!\[([^\]]+)\]\(([^\)]+)\)/g, function (m, c1, c2) {
    return '<img alt="' + c1 + '" src="' + c2 + '">';
  });
  ret = ret.replace(/\[([^\]]+)\]\(([^\)]+)\)/g, function (m, c1, c2) {
    return '<a href="' + c2 + '">' + c1 + '</a>';
  });
  return this.italicize(ret);
}

Markdown.prototype.render_table = function () {

  const self = this;
  const columns = []; // Length is number of columns, values are column widths
  this.state.table.forEach(function (e) {
    e.forEach(function (e, i) {
      const raw = strip_ctrl(e);
      const visible = raw ? raw.length : 0;
      if (columns.length < (i + 1)) {
        columns.push(visible);
      } else if (columns[i] < visible) {
        columns[i] = visible;
      }
    });
  });
  if (this.target == 'html') {
    var ret = '<table>';
    this.state.table.forEach(function (e, i) {
      ret += '<tr>';
      const tag = (i == 0 ? ['<th>', '</th>'] : ['<td>', '</td>']);
      for (var n = 0; n < columns.length; n++) {
        ret += tag[0] + (typeof e[n] == 'undefined' ? '' : e[n]) + tag[1];
      }
      ret += '</tr>';
    });
    ret += '</table><br>';
    this.state.table = [];
    return ret;
  } else {
    // This is pretty bad, but doing it right will be annoying
    // There is no wrapping of cell contents; long lines just get truncated
    var ret = [];
    this.state.table.forEach(function (e, i) {
      var out = '| ';
      for (var n = 0; n < columns.length; n++) {
        var s = e.length < n + 1 ? ' ' : (e[n] == '' ? ' ' : e[n]);
        while (strip_ctrl(s).length < columns[n]) {
          s += ' ';
        }
        out += s + ' | ';
      }
      log('prett ' + out);
      while (strip_ctrl(out).length > self.columns - 2) {
        out = out.substring(0, out.length - 1);
      }
      log('posttt ' + out);
      ret.push(out);
      if (i == 0) {
        out = '|-';
        for (var n = 0; n < columns.length; n++) {
          var s = '';
          while (s.length < columns[n]) {
            s += '-';
          }
          out += s + '-|';
          if (n < columns.length - 1) out += '-';
        }
        while (strip_ctrl(out).length > self.columns - 2) {
          out = out.substring(0, out.length - 1);
        }
        ret.push(out);
      }
    });
    this.state.table = [];
    return ret.join('\r\n') + '\r\n';
  }

}

Markdown.prototype.render_line_console = function (line) {

  var match;
  var ret = '';
  const self = this;

  // Ordered and unordered lists
  match = line.match(/^(\s*)(\*|\d\.)\s+(.+)$/);
  if (match !== null) {
    if (this.state.table.length) ret += this.render_table();
    var lt = (match[2] == '*' ? 'ul' : 'ol');
    if (match[1].length > this.state.list_level) {
      this.state.list_level++;
    } else if (match[1].length < this.state.list_level) {
      this.state.list_level--;
    }
    for (var n = 0; n < this.state.list_level; n++) {
      ret += ' ';
    }
    if (lt == 'ul') {
      ret += match[2];
    } else {
      ret += match[2].substring(0, match[2].length - 1) + ')';
    }
    ret += ' ' + this.render_text_console(match[3]) + '\r\n';
    return ret;
  }
  if (this.state.list_level) {
    ret += '\r\n';
    this.state.list_level = 0;
  }

  row = line.split('|');
  if (row.length > 1) {
    this.state.table.push(row.map(function (e) {
      return self.render_text_console(e);
    }));
    return;
  } else if (this.state.table.length) {
    ret += this.render_table();
  }

  // Heading
  match = line.match(/^#+\s+(.*)$/);
  if (match !== null) {
    return ret + '\1+\1h' + this.render_text_console(match[1]) + '\1-\r\n\r\n';
  }

  // Blockquote
  match = line.match(/^\s*>\s(.+)$/);
  if (match !== null) {
    return ret + quote_msg(
      word_wrap(this.render_text_console(match[1])), this.columns - 1
    ) + '\r\n';
  }

  // Horizontal Rule
  match = line.match(/^---+$/);
  if (match !== null) {
    var s = '';
    while (s.length < this.columns - 1) {
      s += '-';
    }
    return ret + s + '\r\n';
  }

  return ret + this.render_text_console(line) + '\r\n';

}

Markdown.prototype.render_line_html = function (line) {

  var match;
  var ret = '';
  const self = this;

  // Blockquote
  match = line.match(/^\s*>\s(.+)$/);
  if (match !== null) {
    if (this.state.table.length) ret += this.render_table();
    if (!this.state.blockquote) {
      ret += '<blockquote>';
      this.state.blockquote = true;
    }
    return ret + match[1];
  } else if (this.state.blockquote) {
    ret += '</blockquote>';
    this.state.blockquote = false;
  }

  // Ordered and unordered lists
  match = line.match(/^(\s*)(\*|\d\.)\s+(.+)$/);
  if (match !== null) {
    if (this.state.table.length) ret += this.render_table();
    var lt = (match[2] == '*' ? 'ul' : 'ol');
    if (!match[1].length) {
      while (this.state.list_stack.length > 1) {
        ret += '</' + this.state.list_stack.pop() + '>';
      }
      if (this.state.list_stack.length < 1) {
        this.state.list_stack.push(lt);
        ret += '<' + lt + '>';
      }
    } else if (match[1].length > this.state.list_stack.length) {
      this.state.list_stack.push(lt);
      ret += '<' + lt + '>';
    }
    ret += '<li>' + this.render_text_html(match[3]) + '</li>';
    return ret;
  }
  while (this.state.list_stack.length) {
    ret += '</' + this.state.list_stack.pop() + '>';
  }

  row = line.split('|');
  if (row.length > 1) {
    this.state.table.push(row.map(function (e) {
      return self.render_text_html(e);
    }));
    return;
  } else if (this.state.table.length) {
    ret += this.render_table();
  }

  // Heading
  match = line.match(/^#+\s+(.*)$/);
  if (match !== null) {
    var lvl = Math.min(match[0].split(' ')[0].length, 5);
    ret += '<h' + lvl + '>';
    ret += this.render_text_html(match[1]);
    ret += '</h' + lvl + '>';
    return ret;
  }

  // Horizontal Rule
  match = line.match(/^---+$/);
  if (match !== null) {
    return ret + '<hr>';
  }

  return ret + this.render_text_html(line) + '<br>';

}

Markdown.prototype.render_console = function (text) {
  const self = this;
  text.split(/\n/).forEach(function (e) {
    var line = self.render_line_console(e.replace(/\r$/, ''));
    if (typeof line == 'string') {
      self.target.putmsg(line);//.substr(0, self.columns));
    }
  });
  if (this.state.links.length) {
    this.target.putmsg('\1+\1cLinks:\1-\r\n');
    this.state.links.forEach(function (e, i) {
      self.target.putmsg('\1+\1c[' + (i + 1) + '] ' + e.link + '\1-\r\n');
    });
    this.target.putmsg('\r\n');
  }
  if (this.state.images.length) {
    this.target.putmsg('\1+\1mImages:\1-\r\n');
    this.state.images.forEach(function (e, i) {
      self.target.putmsg('\1+\1m[' + (i + 1) + '] ' + e.link + '\1-\r\n');
    });
    this.target.putmsg('\r\n');
  }
}

Markdown.prototype.render_html = function (text) {
  const self = this;
  text.split(/\n/).forEach(function (e) {
    var line = self.render_line_html(e.replace(/\r$/, ''));
    if (typeof line == 'string') writeln(line);
  });
}

Markdown.prototype.render = function (text) {
  if (this.target == 'html') {
    this.render_html(text);
  } else {
    this.render_console(text);
  }
}
