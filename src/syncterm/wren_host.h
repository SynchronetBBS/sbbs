/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef WREN_HOST_H
#define WREN_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bbslist;
struct mouse_event;

/* Per-connection lifecycle. wren_host_init() loads every *.wren file
 * from SYNCTERM_PATH_SCRIPTS into a fresh VM and invokes <Module>.onConnect(bbs)
 * if defined. wren_host_shutdown() releases the VM. Repeated init without
 * intervening shutdown is a no-op. */
void wren_host_init(struct bbslist *bbs);
void wren_host_shutdown(void);

/* Returns true while a VM is loaded with at least one registered hook —
 * used by hot-path call sites to skip dispatcher overhead when nothing
 * is hooked. */
bool wren_host_active(void);

/* Input-shaped dispatchers: return true to consume / drop the event,
 * false to pass through. The first hook returning true short-circuits;
 * subsequent hooks are not called for that event. */
bool wren_host_dispatch_key(int key);
bool wren_host_dispatch_input(unsigned char byte);
bool wren_host_dispatch_output(const void *buf, size_t len);
bool wren_host_dispatch_mouse(struct mouse_event *ev);

/* Status composition: copies a replacement status line into out (size
 * outsz including NUL) and returns true if a script provided one;
 * returns false to use the default. */
bool wren_host_compose_status(const char *def, char *out, size_t outsz);

/* Fires any Hook.every() callbacks whose deadline has elapsed. Called
 * from doterm() just before the main-loop sleep. */
void wren_host_dispatch_timer(void);

#endif /* WREN_HOST_H */
