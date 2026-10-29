/*
 * Subset of DAPLink info.h needed by the CMSIS-DAP core.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INFO_H
#define INFO_H

#ifdef __cplusplus
extern "C" {
#endif

void info_init(void);
const char *info_get_unique_id(void);
const char *info_get_version(void);

#ifdef __cplusplus
}
#endif

#endif
