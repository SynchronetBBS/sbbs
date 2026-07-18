#include "menu_settings.h"

#include "bbslist.h"
#include "uifcinit.h"

#include <ciolib.h>
#include <ini_file.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vidmodes.h>

#include "xpbeep.h"

static bool
valid_output_mode(int mode)
{
	for (size_t i = 0; output_types[i] != NULL; i++) {
		if (output_map[i] == mode)
			return true;
	}
	return false;
}

static bool
valid_audio_modes(unsigned modes)
{
	unsigned known = 0;
	for (size_t i = 0; audio_output_bits[i].name != NULL; i++)
		known |= audio_output_bits[i].bit;
	return (modes & ~known) == 0;
}

static int
kdf_shift(const char *spec)
{
	if (spec == NULL || strncmp(spec, "scrypt-N", 8) != 0)
		return -1;
	char *end = NULL;
	long shift = strtol(spec + 8, &end, 10);
	if (end == spec + 8 || *end != 0 || shift < 8 || shift > 24)
		return -1;
	return (int)shift;
}

static bool
valid_settings(const struct syncterm_settings *set)
{
	if (set->startup_mode < SCREEN_MODE_CURRENT ||
	    set->startup_mode >= SCREEN_MODE_TERMINATOR ||
	    !valid_output_mode(set->output_mode) ||
	    set->defaultCursor < ST_CT_DEFAULT ||
	    set->defaultCursor > ST_CT_SOLID_BLK || set->backlines < 1 ||
	    set->custom_rows < 14 || set->custom_rows > 255 ||
	    set->custom_cols < 40 || set->custom_cols > 255 ||
	    (set->custom_fontheight != 8 && set->custom_fontheight != 14 &&
	    set->custom_fontheight != 16) || set->custom_aw < 1 ||
	    set->custom_ah < 1 || !valid_audio_modes(set->audio_output_modes) ||
	    set->uifc_hclr > 16 || set->uifc_lclr > 16 ||
	    set->uifc_bclr > 8 || set->uifc_cclr > 8 ||
	    set->uifc_lbclr > 16 || set->uifc_lbbclr > 8 ||
	    kdf_shift(set->keyDerivationIterations) < 0)
		return false;
	return true;
}

void
menu_settings_snapshot(struct syncterm_settings *snapshot)
{
	*snapshot = settings;
	/* Web-list ownership remains with the global settings object. */
	snapshot->webgets = NULL;
}

int
menu_settings_scaling_mode(const struct syncterm_settings *snapshot)
{
	if (snapshot->extern_scale)
		return 2;
	return snapshot->blocky ? 0 : 1;
}

void
menu_settings_set_scaling_mode(struct syncterm_settings *snapshot, int mode)
{
	snapshot->blocky = mode == 0;
	snapshot->extern_scale = mode == 2;
}

static str_list_t
read_ini(void)
{
	char path[MAX_PATH + 1];
	get_syncterm_filename(path, sizeof(path), SYNCTERM_PATH_INI, false);
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return strListInit();
	str_list_t result = iniReadFile(fp);
	fclose(fp);
	return result;
}

static bool
write_ini(str_list_t contents)
{
	char path[MAX_PATH + 1];
	get_syncterm_filename(path, sizeof(path), SYNCTERM_PATH_INI, false);
	FILE *fp = fopen(path, "w");
	if (fp == NULL)
		return false;
	bool success = iniWriteFile(fp, contents);
	if (fclose(fp) != 0)
		success = false;
	return success;
}

static void
set_ini_values(str_list_t *ini, const struct syncterm_settings *set)
{
	iniSetBool(ini, "SyncTERM", "ConfirmClose", set->confirm_close,
	    &ini_style);
	iniSetBool(ini, "SyncTERM", "PromptSave", set->prompt_save,
	    &ini_style);
	iniSetEnum(ini, "SyncTERM", "ScreenMode", screen_modes_enum,
	    set->startup_mode, &ini_style);
	iniSetEnum(ini, "SyncTERM", "OutputMode", output_enum,
	    set->output_mode, &ini_style);
	iniSetEnum(ini, "SyncTERM", "DefaultCursor", cursor_enum,
	    set->defaultCursor, &ini_style);
	iniSetBitField(ini, "SyncTERM", "AudioModes", audio_output_bits,
	    set->audio_output_modes, &ini_style);
	iniSetInteger(ini, "SyncTERM", "ScrollBackLines", set->backlines,
	    &ini_style);
	iniSetString(ini, "SyncTERM", "ModemDevice", set->mdm.device_name,
	    &ini_style);
	iniSetLongInt(ini, "SyncTERM", "ModemComRate", set->mdm.com_rate,
	    &ini_style);
	iniSetString(ini, "SyncTERM", "ModemInit", set->mdm.init_string,
	    &ini_style);
	iniSetString(ini, "SyncTERM", "ModemDial", set->mdm.dial_string,
	    &ini_style);
	iniSetString(ini, "SyncTERM", "ListPath", set->stored_list_path,
	    &ini_style);
	iniSetString(ini, "SyncTERM", "TERM", set->TERM, &ini_style);
	iniSetBool(ini, "SyncTERM", "BlockyScaling", set->blocky,
	    &ini_style);
	iniSetBool(ini, "SyncTERM", "ExternalScaling", set->extern_scale,
	    &ini_style);
	iniSetBool(ini, "SyncTERM", "InvertMouseWheel", set->invert_wheel,
	    &ini_style);
	iniSetString(ini, "SyncTERM", "KeyDerivationIterations",
	    set->keyDerivationIterations, &ini_style);
	iniSetInteger(ini, "SyncTERM", "CustomRows", set->custom_rows,
	    &ini_style);
	iniSetInteger(ini, "SyncTERM", "CustomColumns", set->custom_cols,
	    &ini_style);
	iniSetInteger(ini, "SyncTERM", "CustomFontHeight",
	    set->custom_fontheight, &ini_style);
	iniSetInteger(ini, "SyncTERM", "CustomAspectWidth", set->custom_aw,
	    &ini_style);
	iniSetInteger(ini, "SyncTERM", "CustomAspectHeight", set->custom_ah,
	    &ini_style);
	iniSetEnum(ini, "UIFC", "FrameColour", (char **)colour_enum,
	    set->uifc_hclr, &ini_style);
	iniSetEnum(ini, "UIFC", "TextColour", (char **)colour_enum,
	    set->uifc_lclr, &ini_style);
	iniSetEnum(ini, "UIFC", "BackgroundColour", (char **)bg_colour_enum,
	    set->uifc_bclr, &ini_style);
	iniSetEnum(ini, "UIFC", "InverseColour", (char **)bg_colour_enum,
	    set->uifc_cclr, &ini_style);
	iniSetEnum(ini, "UIFC", "LightbarColour", (char **)colour_enum,
	    set->uifc_lbclr, &ini_style);
	iniSetEnum(ini, "UIFC", "LightbarBackgroundColour",
	    (char **)bg_colour_enum, set->uifc_lbbclr, &ini_style);
}

static void
apply_picker_colours(const struct syncterm_settings *set)
{
	uifc.hclr = set->uifc_hclr == 16 ? YELLOW : set->uifc_hclr;
	uifc.lclr = set->uifc_lclr == 16 ? WHITE : set->uifc_lclr;
	uifc.bclr = set->uifc_bclr == 8 ? BLUE : set->uifc_bclr;
	uifc.cclr = set->uifc_cclr == 8 ? CYAN : set->uifc_cclr;
	unsigned fg = set->uifc_lbclr == 16 ? BLUE : set->uifc_lbclr;
	unsigned bg = set->uifc_lbbclr == 8 ? LIGHTGRAY : set->uifc_lbbclr;
	uifc.lbclr = fg | (bg << 4);
}

static void
apply_settings(const struct syncterm_settings *set)
{
	named_string_t **webgets = settings.webgets;
	settings = *set;
	settings.webgets = webgets;
	resolve_list_path(&settings);
	xpbeep_sound_devices_enabled = settings.audio_output_modes;
	ciolib_swap_mouse_butt45 = settings.invert_wheel;
	if (settings.blocky)
		cio_api.options |= CONIO_OPT_BLOCKY_SCALING;
	else
		cio_api.options &= ~CONIO_OPT_BLOCKY_SCALING;
	setscaling_type(settings.extern_scale ? CIOLIB_SCALING_EXTERNAL :
	    CIOLIB_SCALING_INTERNAL);
	int custom = find_vmode(CIOLIB_MODE_CUSTOM);
	if (custom >= 0) {
		struct text_info ti;
		gettextinfo(&ti);
		bool active = ti.currmode == CIOLIB_MODE_CUSTOM;
		if (active)
			textmode(0);
		vparams[custom].cols = settings.custom_cols;
		vparams[custom].rows = settings.custom_rows;
		vparams[custom].charheight = settings.custom_fontheight;
		vparams[custom].aspect_width = settings.custom_aw;
		vparams[custom].aspect_height = settings.custom_ah;
		if (active)
			textmode(CIOLIB_MODE_CUSTOM);
	}
	set_default_cursor();
	apply_picker_colours(&settings);
}

static bool
prepare_settings(const struct syncterm_settings *snapshot,
    struct vmem_cell **resized)
{
	*resized = NULL;
	if (snapshot == NULL || !valid_settings(snapshot))
		return false;
	if ((size_t)snapshot->backlines >
	    SIZE_MAX / 80 / sizeof(*scrollback_buf))
		return false;
	if (snapshot->backlines != settings.backlines) {
		size_t new_cells = (size_t)snapshot->backlines * 80;
		*resized = calloc(new_cells, sizeof(**resized));
		if (*resized == NULL)
			return false;
		if (scrollback_buf != NULL) {
			size_t copy_lines = settings.backlines;
			if (copy_lines > (size_t)snapshot->backlines)
				copy_lines = (size_t)snapshot->backlines;
			memcpy(*resized, scrollback_buf,
			    copy_lines * 80 * sizeof(**resized));
		}
	}
	if (strcmp(snapshot->keyDerivationIterations,
	    settings.keyDerivationIterations) != 0 &&
	    !rewrite_bbslist_kdf(settings.list_path,
	    snapshot->keyDerivationIterations)) {
		free(*resized);
		*resized = NULL;
		return false;
	}
	return true;
}

static void
install_settings(const struct syncterm_settings *snapshot,
    struct vmem_cell *resized)
{
	if (resized != NULL) {
		free(scrollback_buf);
		scrollback_buf = resized;
		if (scrollback_lines > (unsigned)snapshot->backlines)
			scrollback_lines = (unsigned)snapshot->backlines;
	}
	apply_settings(snapshot);
}

bool
menu_settings_apply(const struct syncterm_settings *snapshot)
{
	struct vmem_cell *resized;
	if (!prepare_settings(snapshot, &resized))
		return false;
	install_settings(snapshot, resized);
	return true;
}

bool
menu_settings_save(const struct syncterm_settings *snapshot)
{
	struct vmem_cell *resized;
	if (safe_mode || !prepare_settings(snapshot, &resized))
		return false;
	str_list_t ini = read_ini();
	if (ini == NULL) {
		free(resized);
		return false;
	}
	set_ini_values(&ini, snapshot);
	bool success = write_ini(ini);
	strListFree(&ini);
	if (!success) {
		free(resized);
		return false;
	}
	install_settings(snapshot, resized);
	return true;
}
