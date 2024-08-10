import type { ICgaDefs, INodeDefs, ISbbsDefs } from '@swag/ts4s';
import { load } from '@swag/ts4s';
import * as swindows from 'swindows';
import { getOptions, migrate } from './lib/options';
import UI from './lib/UI';

const cgadefs: ICgaDefs = load('cga_defs.js');
const nodedefs: INodeDefs = load('nodedefs.js');
const sbbsdefs: ISbbsDefs = load('sbbsdefs.js');
const { bbs, console } = js.global;

function init() {
	js.on_exit(`console.attributes = ${console.attributes}`);
	js.on_exit(`bbs.sys_status = ${bbs.sys_status}`);
	js.on_exit('console.home();');
	js.on_exit('console.write("\x1B[0;37;40m")');
	js.on_exit('console.write("\x1B[2J")');
	js.on_exit('console.write("\x1B[?25h");');
	js.time_limit = 0;
	bbs.sys_status |= sbbsdefs.SS_MOFF;
	console.clear(cgadefs.BG_BLACK|cgadefs.LIGHTGRAY);
}

function main(): void {
	migrate();
	const windowManager = new swindows.WindowManager();
	const options = getOptions();
	const ui = new UI(windowManager, options);
	if ((bbs.node_action&nodedefs.NODE_LOGN) === nodedefs.NODE_LOGN && ui.noNewItems) return;
	windowManager.hideCursor();

	while (!js.terminated) {
		windowManager.refresh();
		const input = console.getkey().toLowerCase();
		if (!ui.getCmd(input)) break;
	}
}

init();
main();
