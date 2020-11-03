/**
 * @file attribs.h
 * @author TheSomeMan
 * @date 2020-10-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ATTRIBS_H
#define ATTRIBS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define ATTRIBUTE(attrs) __attribute__(attrs)
#else
#define ATTRIBUTE(attrs)
#endif

#define ATTR_NORETURN ATTRIBUTE((noreturn))
#define ATTR_UNUSED   ATTRIBUTE((unused))

#ifdef __cplusplus
}
#endif

#endif // ATTRIBS_H
