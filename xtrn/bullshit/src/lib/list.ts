import type { INodeDefs, ISmbDefs } from '@swag/ts4s';
import type { IOptions } from './options';
import { load } from '@swag/ts4s';
import { WindowManager } from 'swindows';
import { ILightBarItem } from 'swindows/src/LightBar';

const nodedefs: INodeDefs = load('nodedefs.js');

const smbdefs: ISmbDefs = load('smbdefs.js');
const { file_exists, file_date, format, strftime, time, bbs, system, user, MsgBase, File } = js.global;

const historyFile = format(`${system.data_dir}user/%04d.bullshit`, user.number);

type handler = (title: string, body: string, wrap: boolean) => void;

interface ISortable {
	item: ILightBarItem,
	date: number,
}

interface IUserHistory {
	files: Record<string, number>,
	messages: number[],
}

function formatDate(date: number): string {
	return strftime('%b %d %Y %H:%M', date);
}

function formatItem(text: string, date: number, width: number): string {
	return format(`%-${width - 17}s%s`, text.substring(0, width - 18), formatDate(date));
}

function getUserHistory(): IUserHistory {
	const ret = { files: {}, messages: [] };
	if (!file_exists(historyFile)) return ret;
	const f = new File(historyFile);
	if (!f.open('r')) return ret;
	const history = JSON.parse(f.read()) as IUserHistory;
	f.close();
	return history;
}

function setUserHistory(history: IUserHistory): void {
	const f = new File(historyFile);
	if (!f.open('w')) return;
	f.write(JSON.stringify(history));
	f.close();
}

export function getList(options: IOptions, windowManager: WindowManager, itemHandler: handler): ILightBarItem[] {
	const width = windowManager.size.width - 3
	const sortables: ISortable[] = [];
	const history: IUserHistory = getUserHistory();

	for (const file in options.files) {
		if (!file_exists(options.files[file])) continue;
		if ((bbs.node_action&nodedefs.NODE_LOGN) && options.bullshit.newOnly && history.files[file] !== undefined && file_date(options.files[file]) <= history.files[file]) continue;
		const title = file;
		const date = file_date(options.files[file]);
		const sortable: ISortable = {
			item: {
				text: formatItem(title, date, width),
				onSelect: () => {
					const f = new File(options.files[file]);
					if (!f.open('rb')) return;
					const body = f.read();
					f.close();
					itemHandler(`${title} (${formatDate(date)})`, body, false);
					const history = getUserHistory();
					history.files[file] = time();
					setUserHistory(history);
				}
			},
			date: file_date(options.files[file]),
		};
		sortables.push(sortable);
	}

	const msgBase = new MsgBase(options.bullshit.messageBase);
	if (msgBase.open()) {
		let messages: ISortable[] = [];
		for (let n = msgBase.first_msg; n <= msgBase.last_msg; n++) {
			const h = msgBase.get_msg_header(false, n);
			if (h === null || (h.attr&smbdefs.MSG_DELETE) > 0) continue;
			if ((bbs.node_action&nodedefs.NODE_LOGN) && options.bullshit.newOnly && history.messages.includes(n)) continue;
			const sortable: ISortable = {
				item: {
					text: formatItem(h.subject, h.when_written_time, width),
					onSelect: () => {
						if (!msgBase.open()) return;
						const body = msgBase.get_msg_body(false, n);
						if (body === null) return;
						itemHandler(`${h.subject} (${formatDate(h.when_written_time)})`, body, true);
						const history = getUserHistory();
						if (!history.messages.includes(n)) {
							history.messages.push(n);
							setUserHistory(history);
						}
					}
				},
				date: h.when_written_time,
			};
			messages.push(sortable);
		}
		msgBase.close();
		if (messages.length > options.bullshit.maxMessages) messages = messages.splice(-options.bullshit.maxMessages);
		sortables.push(...messages);
	}

	return sortables.sort((a, b) => b.date - a.date).map(e => e.item);
}
