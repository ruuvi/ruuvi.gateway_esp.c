#ifndef __ESP_ATTR_H__
#define __ESP_ATTR_H__

#define IRAM_ATTR

/* Use IDF_DEPRECATED attribute to mark anything deprecated from use in
   ESP-IDF's own source code, but not deprecated for external users.
*/
#ifdef IDF_CI_BUILD
#define IDF_DEPRECATED(REASON) __attribute__((deprecated(REASON)))
#else
#define IDF_DEPRECATED(REASON)
#endif

#endif // __ESP_ATTR_H__
