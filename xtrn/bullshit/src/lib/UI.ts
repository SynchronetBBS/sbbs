import type { IKeyDefs } from '@swag/ts4s';
import type { IOptions } from './options';
import { load } from '@swag/ts4s';
import * as swindows from 'swindows';
import { getWindowOptions } from './options';
import { getList } from './list';

const keydefs: IKeyDefs = load('key_defs.js');

export default class UI {

	options: IOptions;
	windowManager: swindows.WindowManager;
	listWindow: swindows.ControlledWindow;
	listMenu: swindows.LightBar;
	itemWindow: swindows.ControlledWindow | undefined;
	activeWindow: 'list' | 'item' = 'list';
	noNewItems: boolean = false;

	constructor(windowManager: swindows.WindowManager, options: IOptions) {
		this.options = options;
		this.windowManager = windowManager;

		const listWindowOptions = getWindowOptions(windowManager, options.colors.border, options.bullshit.title, 'Q to quit');
		this.listWindow = new swindows.ControlledWindow(listWindowOptions);

		const items = getList(options, windowManager, this.itemHandler.bind(this));
		if (items.length < 1) {
			items.push({ text: 'No new bulletins', onSelect: () => {} });
			this.noNewItems = true;
		}
		this.listMenu = new swindows.LightBar({
			items,
			window: this.listWindow,
			colors: {
				normal: options.colors.list,
				highlight: options.colors.lightBar
			}
		});
		this.listMenu.draw();
	}

	itemHandler(title: string, body: string, wrap: boolean): void {
		const itemWindowOptions = getWindowOptions(this.windowManager, this.options.colors.border, title, 'Q to quit');
		itemWindowOptions.attr = this.options.colors.text;
		this.itemWindow = new swindows.ControlledWindow(itemWindowOptions);
		if (wrap) {
			this.itemWindow.wrap = swindows.defs.WRAP.WORD;
			this.itemWindow.write(js.global.lfexpand(body));
		} else {
			this.itemWindow.wrap = swindows.defs.WRAP.NONE;
			this.itemWindow.contentWindow.writeAnsi(body, 80);
		}
		this.itemWindow.scrollTo({ x: 0, y: 0 });
		this.activeWindow = 'item';
	}

	getCmd(cmd: string): boolean {
		if (this.activeWindow === 'list') {
			if (cmd === 'q') return false;
			this.listMenu.getCmd(cmd);
		} else if (this.itemWindow !== undefined) {
			switch (cmd) {
				case 'q':
					this.itemWindow?.close();
					this.activeWindow = 'list';
					break;
				case keydefs.KEY_UP:
					this.itemWindow.scroll(0, -1);
					break;
				case keydefs.KEY_PAGEUP:
					this.itemWindow.scroll(0, -this.itemWindow.contentWindow.size.height);
					break;
				case keydefs.KEY_HOME:
					this.itemWindow.scrollTo({ x: 0, y: 0 });
					break;
				case keydefs.KEY_DOWN:
					this.itemWindow.scroll(0, 1);
					break;
				case keydefs.KEY_PAGEDN:
					this.itemWindow.scroll(0, this.itemWindow.contentWindow.size.height);
					break;
				case keydefs.KEY_END:
					this.itemWindow.scrollTo({ x: 0, y: this.itemWindow.contentWindow.dataHeight - this.itemWindow.contentWindow.size.height });
					break;
				default:
					break;
			}
		}
		return true;
	}
}