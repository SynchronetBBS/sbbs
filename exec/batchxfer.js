// Bach File Transfer menu
// Usable as a system "Batch Transfer" loadable module
// i.e. in SCFG->System->Loadable Modules->Batch Transfer

require("sbbsdefs.js", "USER_RIP");
require("file_size.js", "file_size_float");

"use strict";

function batch_list_read(fname)
{
	var f = new File(fname);
	var list;

	if(f.open("r")) {
		list = f.iniGetAllObjects();
		f.close();
	}
	return list;
}

function batch_file_load(f)
{
	var base = new FileBase(f.dir);
	if(base && base.open()) {
		f = base.get(f.name);
		base.close();
	}
	return f;
}

function est_duration(size)
{
	if(!bbs.download_cps)
		return "??:??:??";
	return system.secondstr(size / bbs.download_cps);
}

function batchmenu()
{
	var sort;
	const menu_code = "batchxfr"
	if(bbs.batch_dnload_total < 1 && bbs.batch_upload_total < 1 && file_area.upload_dir == undefined) {
		console.print(bbs.text(bbs.text.NoFilesInBatchQueue));
		return;
	}
	if(console.term_supports(USER_RIP) && !(user.settings & USER_EXPERT))
		bbs.menu("batchxfr");
	while(bbs.online && (file_area.upload_dir !== undefined || bbs.batch_dnload_total || bbs.batch_upload_total)) {
		if(!console.term_supports(USER_RIP) && !(user.settings & USER_EXPERT)) {
			console.aborted = false;
			bbs.menu("batchxfr");
		}
		bbs.nodesync();
		console.print(bbs.text(bbs.text.BatchMenuPrompt));
		const keys = "CDLRU?\r" + console.quit_key;
		var ch = console.getkeys(keys, 0);
		switch(ch) {
			case '?':
				if((user.settings & USER_EXPERT) || console.term_supports(USER_RIP))
					bbs.menu("batchxfr");
				break;
			case 'C':
				if(bbs.batch_upload_total < 1) {
					console.print(bbs.text(bbs.text.UploadQueueIsEmpty));
				} else {
					if(bbs.text(bbs.text.ClearUploadQueueQ)[0]==0 || !console.noyes(bbs.text(bbs.text.ClearUploadQueueQ))) {
						if(bbs.batch_clear(/* upload */true))
							console.print(bbs.text(bbs.text.UploadQueueCleared));
					}
				}
				if(bbs.batch_dnload_total <1 ) {
					console.print(bbs.text(bbs.text.DownloadQueueIsEmpty));
				} else {
					if(bbs.text(bbs.text.ClearDownloadQueueQ)[0]==0 || !console.noyes(bbs.text(bbs.text.ClearDownloadQueueQ))) {
						if(bbs.batch_clear(/* upload */false))
							console.print(bbs.text(bbs.text.DownloadQueueCleared));
					}
				}
				break;
			case 'D':
				bbs.batch_download();
				break;
			case 'L':
				var list = batch_list_read(user.batch_upload_list);
				if(list && list.length) {
					if(sort === undefined)
						sort = console.yesno(bbs.text(bbs.text.SortAlphaQ));
					if(sort)
						bbs.batch_sort(/* upload */true);
					console.print(bbs.text(bbs.text.UploadQueueLstHdr));
					for(var i in list) {
						var f = list[i];
						console.print(format(bbs.text(bbs.text.UploadQueueLstFmt)
							,Number(i) + 1
							,f.name
							,f.desc || bbs.text(bbs.text.NoDescription)));
						if(console.aborted)
							break;
					}
				} else
					console.print(bbs.text(bbs.text.UploadQueueIsEmpty));

				var totalsize = 0;
				var totalcdt = 0;
				list = batch_list_read(user.batch_download_list);
				if(list && list.length) {
					if(sort === undefined)
						sort = console.yesno(bbs.text(bbs.text.SortAlphaQ));
					if(sort)
						bbs.batch_sort(/* upload */false);
					console.print(bbs.text(bbs.text.DownloadQueueLstHdr));
					for(var i in list) {
						var f = batch_file_load(list[i]);
						console.print(format(bbs.text(bbs.text.DownloadQueueLstFmt)
							,Number(i) + 1
							,f.name
							,file_size_float(f.cost, 1, 1)
							,file_size_float(f.size, 1, 1)
							,est_duration(f.size)
							,system.datestr(f.time)));
						totalsize += f.size;
						totalcdt += f.cost;
						if(console.aborted)
							break;
					}
					if(!console.aborted)
						console.print(format(bbs.text(bbs.text.DownloadQueueTotals)
							,file_size_float(totalcdt, 1, 1)
							,file_size_float(totalsize, 1, 1)
							,est_duration(totalsize)));
				} else
					console.print(bbs.text(bbs.text.DownloadQueueIsEmpty));
				break;
			case 'R':
				var n;
				if((n = bbs.batch_upload_total) > 0) {
					console.print(format(bbs.text(bbs.text.RemoveWhichFromUlQueue), n));
					var str = console.getstr();
					if((n = parseInt(str)) > 0)
						n = bbs.batch_remove(/* upload */true, n - 1);
					else
						n = bbs.batch_remove(/* upload */true, str);
					console.print(format(bbs.text(bbs.text.NFilesRemoved), n));
				}
				if((n = bbs.batch_dnload_total) > 0) {
					console.print(format(bbs.text(bbs.text.RemoveWhichFromDlQueue), n));
					var str = console.getstr();
					if((n = parseInt(str)) > 0)
						n = bbs.batch_remove(/* upload */false, n - 1);
					else
						n = bbs.batch_remove(/* upload */false, str);
					console.print(format(bbs.text(bbs.text.NFilesRemoved), n));
				}
				break;
			case 'U':
				if(user.security.restrictions & UFLAG_U) {
					console.print(bbs.text(bbs.text.R_Upload));
					break;
				}
				bbs.batch_upload();
				break;
			default:
				return;
		}
	}
}

batchmenu();

