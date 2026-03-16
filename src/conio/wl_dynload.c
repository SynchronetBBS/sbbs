/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/*
 * This file must NOT include wl_dynload.h because that header defines
 * macros that redirect function names (e.g. wl_display_connect ->
 * wl_dynfn.wl_display_connect).  Those macros would break the LOAD()
 * macro below which needs the raw symbol names for dlsym() strings.
 *
 * Instead, we include wayland-client-core.h for the type declarations
 * and define/extern the struct ourselves.
 */

#include <dlfcn.h>
#include <string.h>
#include <wayland-client-core.h>

/* Must match the struct in wl_dynload.h */
struct wl_dynload_funcs {
	struct wl_proxy *(*wl_proxy_marshal_flags)(struct wl_proxy *proxy,
	    uint32_t opcode, const struct wl_interface *interface,
	    uint32_t version, uint32_t flags, ...);
	int (*wl_proxy_add_listener)(struct wl_proxy *proxy,
	    void (**implementation)(void), void *data);
	void (*wl_proxy_destroy)(struct wl_proxy *proxy);
	uint32_t (*wl_proxy_get_version)(struct wl_proxy *proxy);
	void *(*wl_proxy_get_user_data)(struct wl_proxy *proxy);
	void (*wl_proxy_set_user_data)(struct wl_proxy *proxy, void *user_data);
	struct wl_display *(*wl_display_connect)(const char *name);
	void (*wl_display_disconnect)(struct wl_display *display);
	int (*wl_display_get_fd)(struct wl_display *display);
	int (*wl_display_dispatch)(struct wl_display *display);
	int (*wl_display_dispatch_pending)(struct wl_display *display);
	int (*wl_display_roundtrip)(struct wl_display *display);
	int (*wl_display_flush)(struct wl_display *display);
	int (*wl_display_prepare_read)(struct wl_display *display);
	int (*wl_display_read_events)(struct wl_display *display);
	void (*wl_display_cancel_read)(struct wl_display *display);
};

struct wl_dynload_funcs wl_dynfn;

static void *wl_dl_handle;

void
wl_dynload_close(void)
{
	if (wl_dl_handle) {
		dlclose(wl_dl_handle);
		wl_dl_handle = NULL;
	}
	memset(&wl_dynfn, 0, sizeof(wl_dynfn));
}

int
wl_dynload_init(void)
{
	if (wl_dl_handle)
		return 0;

	wl_dl_handle = dlopen("libwayland-client.so", RTLD_LAZY);
	if (!wl_dl_handle)
		wl_dl_handle = dlopen("libwayland-client.so.0", RTLD_LAZY);
	if (!wl_dl_handle)
		return -1;

#define LOAD(name) do { \
	wl_dynfn.name = dlsym(wl_dl_handle, #name); \
	if (!wl_dynfn.name) { \
		wl_dynload_close(); \
		return -1; \
	} \
} while (0)

	LOAD(wl_proxy_marshal_flags);
	LOAD(wl_proxy_add_listener);
	LOAD(wl_proxy_destroy);
	LOAD(wl_proxy_get_version);
	LOAD(wl_proxy_get_user_data);
	LOAD(wl_proxy_set_user_data);
	LOAD(wl_display_connect);
	LOAD(wl_display_disconnect);
	LOAD(wl_display_get_fd);
	LOAD(wl_display_dispatch);
	LOAD(wl_display_dispatch_pending);
	LOAD(wl_display_roundtrip);
	LOAD(wl_display_flush);
	LOAD(wl_display_prepare_read);
	LOAD(wl_display_read_events);
	LOAD(wl_display_cancel_read);

#undef LOAD

	return 0;
}
