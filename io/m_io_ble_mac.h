/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __M_IO_BLE_MAC_H__
#define __M_IO_BLE_MAC_H__

#include <mstdlib/mstdlib_io.h>
#include "m_io_ble_int.h"

#import <CoreBluetooth/CoreBluetooth.h>

typedef struct {
	CFTypeRef     peripheral; /* CBPeripheral */
	char         *name;
	char         *mac;
	M_list_str_t *services;   /* key = name, val = uuid. Sorted. */
	M_time_t      last_seen;  /* Over 30 minutes remove. Check after scan finishes. */
	M_io_layer_t *layer;      /* When in use the layer using the peripheral. */
} M_io_ble_device_t;

void M_io_ble_cbc_event_reset(void);
void M_io_ble_cache_device(CFTypeRef peripheral);
void M_io_ble_device_add_serivce(const char *mac, const char *uuid);
void M_io_ble_device_clear_services(const char *mac);

M_bool M_io_ble_device_need_read_services(CBPeripheral *peripheral);

#endif /* __M_IO_BLE_MAC_H__ */
