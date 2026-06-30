/*-
 * Subset of evdev key codes from FreeBSD's
 * /usr/include/dev/evdev/input-event-codes.h.
 *
 * Copyright (c) 2016 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * Copyright (c) 2015-2016 Vladimir Kondratyev <wulf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _EVDEV_CODES_H_
#define _EVDEV_CODES_H_

#define EVDEV_KEY_RESERVED        0
#define EVDEV_KEY_ESC             1
#define EVDEV_KEY_1               2
#define EVDEV_KEY_2               3
#define EVDEV_KEY_3               4
#define EVDEV_KEY_4               5
#define EVDEV_KEY_5               6
#define EVDEV_KEY_6               7
#define EVDEV_KEY_7               8
#define EVDEV_KEY_8               9
#define EVDEV_KEY_9               10
#define EVDEV_KEY_0               11
#define EVDEV_KEY_MINUS           12
#define EVDEV_KEY_EQUAL           13
#define EVDEV_KEY_BACKSPACE       14
#define EVDEV_KEY_TAB             15
#define EVDEV_KEY_Q               16
#define EVDEV_KEY_W               17
#define EVDEV_KEY_E               18
#define EVDEV_KEY_R               19
#define EVDEV_KEY_T               20
#define EVDEV_KEY_Y               21
#define EVDEV_KEY_U               22
#define EVDEV_KEY_I               23
#define EVDEV_KEY_O               24
#define EVDEV_KEY_P               25
#define EVDEV_KEY_LEFTBRACE       26
#define EVDEV_KEY_RIGHTBRACE      27
#define EVDEV_KEY_ENTER           28
#define EVDEV_KEY_LEFTCTRL        29
#define EVDEV_KEY_A               30
#define EVDEV_KEY_S               31
#define EVDEV_KEY_D               32
#define EVDEV_KEY_F               33
#define EVDEV_KEY_G               34
#define EVDEV_KEY_H               35
#define EVDEV_KEY_J               36
#define EVDEV_KEY_K               37
#define EVDEV_KEY_L               38
#define EVDEV_KEY_SEMICOLON       39
#define EVDEV_KEY_APOSTROPHE      40
#define EVDEV_KEY_GRAVE           41
#define EVDEV_KEY_LEFTSHIFT       42
#define EVDEV_KEY_BACKSLASH       43
#define EVDEV_KEY_Z               44
#define EVDEV_KEY_X               45
#define EVDEV_KEY_C               46
#define EVDEV_KEY_V               47
#define EVDEV_KEY_B               48
#define EVDEV_KEY_N               49
#define EVDEV_KEY_M               50
#define EVDEV_KEY_COMMA           51
#define EVDEV_KEY_DOT             52
#define EVDEV_KEY_SLASH           53
#define EVDEV_KEY_RIGHTSHIFT      54
#define EVDEV_KEY_KPASTERISK      55
#define EVDEV_KEY_LEFTALT         56
#define EVDEV_KEY_SPACE           57
#define EVDEV_KEY_CAPSLOCK        58
#define EVDEV_KEY_F1              59
#define EVDEV_KEY_F2              60
#define EVDEV_KEY_F3              61
#define EVDEV_KEY_F4              62
#define EVDEV_KEY_F5              63
#define EVDEV_KEY_F6              64
#define EVDEV_KEY_F7              65
#define EVDEV_KEY_F8              66
#define EVDEV_KEY_F9              67
#define EVDEV_KEY_F10             68
#define EVDEV_KEY_NUMLOCK         69
#define EVDEV_KEY_SCROLLLOCK      70
#define EVDEV_KEY_KP7             71
#define EVDEV_KEY_KP8             72
#define EVDEV_KEY_KP9             73
#define EVDEV_KEY_KPMINUS         74
#define EVDEV_KEY_KP4             75
#define EVDEV_KEY_KP5             76
#define EVDEV_KEY_KP6             77
#define EVDEV_KEY_KPPLUS          78
#define EVDEV_KEY_KP1             79
#define EVDEV_KEY_KP2             80
#define EVDEV_KEY_KP3             81
#define EVDEV_KEY_KP0             82
#define EVDEV_KEY_KPDOT           83
#define EVDEV_KEY_102ND           86
#define EVDEV_KEY_F11             87
#define EVDEV_KEY_F12             88
#define EVDEV_KEY_KPENTER         96
#define EVDEV_KEY_RIGHTCTRL       97
#define EVDEV_KEY_KPSLASH         98
#define EVDEV_KEY_SYSRQ           99
#define EVDEV_KEY_RIGHTALT        100
#define EVDEV_KEY_HOME            102
#define EVDEV_KEY_UP              103
#define EVDEV_KEY_PAGEUP          104
#define EVDEV_KEY_LEFT            105
#define EVDEV_KEY_RIGHT           106
#define EVDEV_KEY_END             107
#define EVDEV_KEY_DOWN            108
#define EVDEV_KEY_PAGEDOWN        109
#define EVDEV_KEY_INSERT          110
#define EVDEV_KEY_DELETE          111
#define EVDEV_KEY_PAUSE           119
#define EVDEV_KEY_LEFTMETA        125
#define EVDEV_KEY_RIGHTMETA       126
#define EVDEV_KEY_MENU            139
#define EVDEV_KEY_F13             183
#define EVDEV_KEY_F14             184
#define EVDEV_KEY_F15             185
#define EVDEV_KEY_F16             186
#define EVDEV_KEY_F17             187
#define EVDEV_KEY_F18             188
#define EVDEV_KEY_F19             189
#define EVDEV_KEY_F20             190
#define EVDEV_KEY_F21             191
#define EVDEV_KEY_F22             192
#define EVDEV_KEY_F23             193
#define EVDEV_KEY_F24             194

#define EVDEV_KEY_MAX             0x2ff

#endif
