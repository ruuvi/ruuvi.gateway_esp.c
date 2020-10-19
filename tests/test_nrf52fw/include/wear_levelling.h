// Copyright 2015-2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _wear_levelling_H_
#define _wear_levelling_H_

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief wear levelling handle
 */
typedef int32_t wl_handle_t;

#define WL_INVALID_HANDLE -1

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _wear_levelling_H_
