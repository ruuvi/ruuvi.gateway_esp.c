#ifndef _MQTT_H_INCLUDED
#define _MQTT_H_INCLUDED

#include "ruuvidongle.h"

void mqtt_app_start (void);
void mqtt_publish_table (struct adv_report_table * table);

#endif
