/*
 * TODO: built-in magic names... FILES and NEW
 * FILES lists all FREQable files and
 * NEW lists ones newer than 10 days.
 */

require("filebase.js", 'FileBase');

var FREQIT = {
	dircache:{},
	added:{},
	files:0,
	reset:function()
	{
		this.added = {};
		this.files = 0;
	},
	handle_magic:function (magic, cb_data, protected, pw, cfg)
	{
		var file=undefined;

		if (magic.secure && !protected)
			return;
		if (this.dircache[magic.dir] === undefined)
			this.dircache[magic.dir] = new FileBase(magic.dir);
		this.dircache[magic.dir].forEach(function(fent) {
			if (wildmatch(fent.name, magic.match, true)) {
				if (file === undefined || fent.uldate > file.uldate)
					file = fent;
			}
		});
		if (file !== undefined) {
			this.add_file(file.path, cb_data, cfg);
			return 1;
		}
		return 0;
	},
	handle_regular:function (match, cb_data, protected, pw, cfg)
	{
		var file=undefined;
		var count=0;

        const self = this;
		function handle_list(list) {
			list.forEach(function(dir) {
				if (self.dircache[dir] === undefined)
					self.dircache[dir] = new FileBase(dir);
				self.dircache[dir].forEach(function(fent) {
					if (wildmatch(fent.name, match, true))
						self.add_file(fent.path, cb_data);
				});
			});
		}

		if (protected)
			handle_list(cfg.securedirs);
		handle_list(cfg.dirs);
	}
};
