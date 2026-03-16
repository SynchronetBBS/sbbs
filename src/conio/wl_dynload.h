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
 * Dynamic loading of libwayland-client.
 *
 * The generated protocol headers (wl_wayland-client-protocol.h,
 * wl_xdg-shell-client-protocol.h, wl_viewporter-client-protocol.h)
 * contain inline functions that call wl_proxy_marshal_flags(),
 * wl_proxy_add_listener(), etc.  Rather than linking libwayland-client
 * at build time, we dlopen it at runtime and redirect all calls through
 * function pointers using preprocessor macros.
 *
 * Include order:
 *   1. <wayland-client-core.h>   — struct/type declarations
 *   2. "wl_dynload.h"           — macros that redirect function names
 *   3. generated protocol headers — inline functions use our macros
 *
 * Protocol files were generated from XML with wayland-scanner 1.24.0:
 *
 *   Core (wayland.xml from /usr/local/share/wayland/wayland.xml):
 *     wayland-scanner -c client-header wayland.xml wl_wayland-client-protocol.h
 *     wayland-scanner -c private-code  wayland.xml wl_wayland-protocol.c
 *
 *   xdg-shell (from wayland-protocols stable/xdg-shell/xdg-shell.xml):
 *     wayland-scanner -c client-header xdg-shell.xml wl_xdg-shell-client-protocol.h
 *     wayland-scanner -c private-code  xdg-shell.xml wl_xdg-shell-protocol.c
 *
 *   viewporter (from wayland-protocols stable/viewporter/viewporter.xml):
 *     wayland-scanner -c client-header viewporter.xml wl_viewporter-client-protocol.h
 *     wayland-scanner -c private-code  viewporter.xml wl_viewporter-protocol.c
 *
 * To regenerate (e.g. when adding xkbcommon or new protocols):
 *   WL_XML=/usr/local/share/wayland/wayland.xml
 *   WL_PROTO=$(pkg-config --variable=pkgdatadir wayland-protocols)
 *   wayland-scanner -c client-header $WL_XML wl_wayland-client-protocol.h
 *   wayland-scanner -c private-code  $WL_XML wl_wayland-protocol.c
 *   wayland-scanner -c client-header $WL_PROTO/stable/xdg-shell/xdg-shell.xml wl_xdg-shell-client-protocol.h
 *   wayland-scanner -c private-code  $WL_PROTO/stable/xdg-shell/xdg-shell.xml wl_xdg-shell-protocol.c
 *   wayland-scanner -c client-header $WL_PROTO/stable/viewporter/viewporter.xml wl_viewporter-client-protocol.h
 *   wayland-scanner -c private-code  $WL_PROTO/stable/viewporter/viewporter.xml wl_viewporter-protocol.c
 */

#ifndef WL_DYNLOAD_H
#define WL_DYNLOAD_H

#include <stdint.h>
#include <wayland-client-core.h>

struct wl_dynload_funcs {
	/* wl_proxy functions (used by generated protocol inline functions) */
	struct wl_proxy *(*wl_proxy_marshal_flags)(struct wl_proxy *proxy,
	    uint32_t opcode, const struct wl_interface *interface,
	    uint32_t version, uint32_t flags, ...);
	int (*wl_proxy_add_listener)(struct wl_proxy *proxy,
	    void (**implementation)(void), void *data);
	void (*wl_proxy_destroy)(struct wl_proxy *proxy);
	uint32_t (*wl_proxy_get_version)(struct wl_proxy *proxy);
	void *(*wl_proxy_get_user_data)(struct wl_proxy *proxy);
	void (*wl_proxy_set_user_data)(struct wl_proxy *proxy, void *user_data);

	/* wl_display functions (called directly by event thread) */
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

extern struct wl_dynload_funcs wl_dynfn;

int wl_dynload_init(void);
void wl_dynload_close(void);

/*
 * Redirect all wl_proxy_* and wl_display_* calls through function pointers.
 * This must appear AFTER wayland-client-core.h and BEFORE generated protocol
 * headers so the inline functions in those headers call through wl_dynfn.
 */
#define wl_proxy_marshal_flags    wl_dynfn.wl_proxy_marshal_flags
#define wl_proxy_add_listener     wl_dynfn.wl_proxy_add_listener
#define wl_proxy_destroy          wl_dynfn.wl_proxy_destroy
#define wl_proxy_get_version      wl_dynfn.wl_proxy_get_version
#define wl_proxy_get_user_data    wl_dynfn.wl_proxy_get_user_data
#define wl_proxy_set_user_data    wl_dynfn.wl_proxy_set_user_data
#define wl_display_connect        wl_dynfn.wl_display_connect
#define wl_display_disconnect     wl_dynfn.wl_display_disconnect
#define wl_display_get_fd         wl_dynfn.wl_display_get_fd
#define wl_display_dispatch       wl_dynfn.wl_display_dispatch
#define wl_display_dispatch_pending wl_dynfn.wl_display_dispatch_pending
#define wl_display_roundtrip      wl_dynfn.wl_display_roundtrip
#define wl_display_flush          wl_dynfn.wl_display_flush
#define wl_display_prepare_read   wl_dynfn.wl_display_prepare_read
#define wl_display_read_events    wl_dynfn.wl_display_read_events
#define wl_display_cancel_read    wl_dynfn.wl_display_cancel_read

#endif
