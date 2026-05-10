/* Copyright (C), 2026 by Stephen Hurd */

#ifndef _CONN_MQTT_H_
#define _CONN_MQTT_H_

#include "bbslist.h"

int  mqtt_connect(struct bbslist *bbs);
int  mqtt_close(void);

#endif
