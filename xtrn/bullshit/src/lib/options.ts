import type { ICgaDefs } from '@swag/ts4s';
import { load } from '@swag/ts4s';
import * as swindows from 'swindows';

const cgadefs: ICgaDefs = load('cga_defs.js');
const { file_exists, file_rename, system, File } = js.global;

function parseColorOption(color: string): swindows.types.attr {
	const c = color.split('|');
	let ret = cgadefs[c[0] as keyof typeof cgadefs] as swindows.types.attr;
	if (c.length > 1) ret |= (cgadefs[c[1] as keyof typeof cgadefs] as swindows.types.attr);
	if ((ret & (7 << 4)) === 0) ret |= cgadefs.BG_BLACK;
	return ret as swindows.types.attr;
}

export interface IOptions {
	bullshit: {
		messageBase: string,
		maxMessages: number,
		newOnly: boolean,
		title: string
	},
	colors: {
		title: swindows.types.attr,
		text: swindows.types.attr,
		heading: swindows.types.attr,
		lightBar: swindows.types.attr,
		list: swindows.types.attr,
		border: swindows.types.attr[],
	},
	files: Record<string, string>,
}

export function migrate(): void {
	const oldFile = `${js.exec_dir}bullshit.ini`;
	if (!file_exists(oldFile)) return;
	let f = new File(oldFile);
	if (!f.open('r')) return;
	const oldRoot = f.iniGetObject();
	const oldColors = f.iniGetObject('colors');
	const oldFiles = f.iniGetObject('files');
	f.close();

	const opts = {
		bullshit: {
			messageBase: oldRoot?.messageBase ?? '',
			maxMessages: oldRoot?.maxMessages ?? 100,
			newOnly: oldRoot?.newOnly === undefined ? false : true,
			title: 'Bulletins',
		},
		colors: {
			title: (oldColors?.title as string) ?? 'WHITE',
			text: (oldColors?.text as string) ?? 'LIGHTGRAY',
			lightBar: `${(oldColors?.lightbarForeground as string) ?? 'WHITE'}|${(oldColors?.lightbarBackground as string) ?? 'BG_CYAN'}`,
			border: oldColors?.border ?? 'WHITE,LIGHTCYAN,CYAN,LIGHTBLUE',
		},
		files: oldFiles ?? {},
	};

	f = new File(`${system.ctrl_dir}modopts.d/bullshit.ini`);
	if (!f.open(f.exists ? 'r+' : 'w+')) return;
	f.iniSetObject('bullshit', opts.bullshit);
	f.iniSetObject('bullshit:colors', opts.colors);
	f.iniSetObject('bullshit:files', opts.files);
	f.close();

	file_rename(oldFile, `${oldFile}.old`);
}

export function getOptions(): IOptions {
	const ret: IOptions = {
		bullshit: {
			messageBase: '',
			maxMessages: 100,
			newOnly: false,
			title: 'Bulletins',
		},
		colors: {
			title: (cgadefs.BG_BLACK|cgadefs.WHITE) as swindows.types.attr,
			text: (cgadefs.BG_BLACK|cgadefs.LIGHTGRAY) as swindows.types.attr,
			heading: (cgadefs.BG_BLACK|cgadefs.DARKGRAY) as swindows.types.attr,
			lightBar: (cgadefs.BG_CYAN|cgadefs.WHITE) as swindows.types.attr,
			list: (cgadefs.BG_BLACK|cgadefs.LIGHTGRAY) as swindows.types.attr,
			border: [
				((cgadefs.BG_BLACK|cgadefs.WHITE) as swindows.types.attr),
				((cgadefs.BG_BLACK|cgadefs.LIGHTCYAN) as swindows.types.attr),
				((cgadefs.BG_BLACK|cgadefs.CYAN) as swindows.types.attr),
				((cgadefs.BG_BLACK|cgadefs.LIGHTBLUE) as swindows.types.attr),
			]
		},
		files: {},
	};

	const f = new File(`${system.ctrl_dir}modopts.d/bullshit.ini`);
	if (!f.open('r')) throw new Error(`Failed to open ${f.name} for reading`);
	const bullshitIni = f.iniGetObject('bullshit');
	const colorsIni = f.iniGetObject('bullshit:colors');
	const filesIni = f.iniGetObject('bullshit:files');
	f.close();

	if (typeof bullshitIni?.messageBase === 'string') ret.bullshit.messageBase = bullshitIni.messageBase;
	if (typeof bullshitIni?.maxMessages === 'number') ret.bullshit.maxMessages = bullshitIni.maxMessages;
	if (ret.bullshit.maxMessages < 1) ret.bullshit.maxMessages = Infinity;
	if (typeof bullshitIni?.newOnly === 'boolean') ret.bullshit.newOnly = bullshitIni.newOnly;
	if (typeof bullshitIni?.title === 'string') ret.bullshit.title = bullshitIni.title;

	if (typeof colorsIni?.title === 'string') ret.colors.title = parseColorOption(colorsIni.title);
	if (typeof colorsIni?.text === 'string') ret.colors.title = parseColorOption(colorsIni.text);
	if (typeof colorsIni?.heading === 'string') ret.colors.title = parseColorOption(colorsIni.heading);
	if (typeof colorsIni?.lightBar === 'string') ret.colors.title = parseColorOption(colorsIni.lightBar);
	if (typeof colorsIni?.list === 'string') ret.colors.title = parseColorOption(colorsIni.list);
	if (typeof colorsIni?.border === 'string' && colorsIni.border.length > 0) {
		ret.colors.border = [];
		const border = colorsIni.border.split(',');
		for (const b of border) {
			ret.colors.border.push(parseColorOption(b));
		}
	}

	for (const file in filesIni) {
		ret.files[file] = filesIni[file] as string;
	}

	return ret;
}

export function getWindowOptions(windowManager: swindows.WindowManager, border: swindows.types.attr[], title?: string, footer?: string): swindows.types.IControlledWindowOptions {
	return {
		border: {
			style: swindows.defs.BORDER_STYLE.SINGLE,
			pattern: swindows.defs.BORDER_PATTERN.DIAGONAL,
			attr: border,
		},
		title: { 
			text: title ?? '',
			attr: ((cgadefs.BG_BLACK|cgadefs.WHITE) as swindows.types.attr),
		},
		footer: { 
			text: footer ?? '',
			attr: ((cgadefs.BG_BLACK|cgadefs.WHITE) as swindows.types.attr),
			alignment: swindows.defs.ALIGNMENT.RIGHT,
		},
		position: { x: 0, y: 0 },
		size: {
			width: windowManager.size.width,
			height: windowManager.size.height
		},
		scrollBar: {
			vertical: {
				enabled: true,
			}
		},
		windowManager,
		name: '',
	}
};
