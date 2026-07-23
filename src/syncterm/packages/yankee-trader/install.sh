#!/bin/sh

set -eu

YT_PACKAGE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ "$#" -gt 1 ]; then
	echo "Usage: $0 [SyncTERM-scripts-directory]" >&2
	exit 2
fi

if [ "$#" -eq 1 ]; then
	YT_SCRIPT_DIR=$1
else
	case "$(uname -s)" in
		Darwin)
			YT_SCRIPT_DIR="$HOME/Library/Application Support/SyncTERM/scripts"
			;;
		Haiku)
			YT_SCRIPT_DIR="$HOME/config/settings/SyncTERM/scripts"
			;;
		*)
			YT_DATA_DIR=${XDG_DATA_HOME:-"$HOME/.local/share"}
			YT_SCRIPT_DIR="$YT_DATA_DIR/syncterm/scripts"
			;;
	esac
fi

YT_MENU_DIR="$YT_SCRIPT_DIR/auto/menu"
mkdir -p "$YT_SCRIPT_DIR" "$YT_MENU_DIR"

for YT_MODULE in yankee_trader.wren yankee_trader_calc.wren \
    yankee_trader_guide.wren yankee_trader_offline.wren \
    yankee_trader_state.wren yankee_trader_tools.wren \
    yankee_trader_ui.wren; do
	YT_SOURCE="$YT_PACKAGE_DIR/scripts/$YT_MODULE"
	YT_TARGET="$YT_SCRIPT_DIR/$YT_MODULE"
	if [ -f "$YT_TARGET" ] && ! cmp -s "$YT_SOURCE" "$YT_TARGET"; then
		cp -p "$YT_TARGET" "$YT_TARGET.bak"
	fi
	cp "$YT_SOURCE" "$YT_TARGET"
done

YT_SOURCE="$YT_PACKAGE_DIR/scripts/auto/menu/yankee_trader_menu.wren"
YT_TARGET="$YT_MENU_DIR/yankee_trader_menu.wren"
if [ -f "$YT_TARGET" ] && ! cmp -s "$YT_SOURCE" "$YT_TARGET"; then
	cp -p "$YT_TARGET" "$YT_TARGET.bak"
fi
cp "$YT_SOURCE" "$YT_TARGET"

echo "Installed Yankee Trader Wren package in: $YT_SCRIPT_DIR"
echo "Add 'yankee_trader' to the BBS entry's Wren Scripts list."
echo "Alt-Y also opens the offline planner from SyncTERM's main menu."
