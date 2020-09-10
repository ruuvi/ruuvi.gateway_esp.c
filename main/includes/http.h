#ifndef _HTTP_H_DEF_
#define _HTTP_H_DEF_

#include "ruuvi_gateway.h"

void
http_send(const char *msg);

void
http_send_advs(struct adv_report_table *);

#endif
