#ifndef _MQTT_H_INCLUDED
#define _MQTT_H_INCLUDED

#include "ruuvi_gateway.h"

void
mqtt_app_start(void);

void
mqtt_publish_table(struct adv_report_table *table);

#endif
