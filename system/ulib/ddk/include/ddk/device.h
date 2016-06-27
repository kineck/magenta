// Copyright 2016 The Fuchsia Authors
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

#pragma once

#include <magenta/syscalls.h>
#include <magenta/types.h>
#include <mxu/list.h>

typedef struct mx_device mx_device_t;
typedef struct mx_driver mx_driver_t;
typedef struct mx_device_prop mx_device_prop_t;

typedef struct mx_protocol_device mx_protocol_device_t;

typedef struct vnode vnode_t;

//TODO: multi-char constants are implementation-specific
//      move to something more ABI-stable

#define MX_DEVICE_MAGIC 'MDEV'
#define MX_DEVICE_NAME_MAX 32

struct mx_device {
    uintptr_t magic;

    const char* name;

    mx_protocol_device_t* ops;

    uint32_t flags;
    uint32_t refcount;

    mx_handle_t event;
    mx_handle_t remote;
    uintptr_t remote_id;

    // most devices implement a single
    // protocol beyond the base device protocol
    uint32_t protocol_id;
    void* protocol_ops;

    mx_driver_t* driver;
    // driver that has published this device

    mx_device_t* parent;
    // parent in the device tree

    mx_driver_t* owner;
    // driver that is bound to this device, NULL if unbound

    struct list_node node;
    // for the parent's device_list

    struct list_node children;
    // list of this device's children in the device tree

    struct list_node unode;
    // for list of all unmatched devices, if not bound
    // TODO: use this for general lifecycle tracking

    vnode_t* vnode;
    // used by devmgr internals

    mx_device_prop_t* props;
    uint32_t prop_count;
    // properties for driver binding

    char namedata[MX_DEVICE_NAME_MAX + 1];
};

// mx_device_t objects must be created or initialized by the driver manager's
// device_create() and device_init() functions.  Drivers MAY NOT touch any
// fields in the mx_device_t, except for the protocol_id and protocol_ops
// fields which it may fill out after init and before device_add() is called.

// The Device Protocol
typedef struct mx_protocol_device {
    mx_status_t (*get_protocol)(mx_device_t* dev, uint32_t proto_id, void** protocol);
    // Asks if the device supports a specific protocol.
    // If it does, protocol ops returned via **protocol.

    mx_status_t (*open)(mx_device_t* dev, uint32_t flags);

    mx_status_t (*close)(mx_device_t* dev);

    mx_status_t (*release)(mx_device_t* dev);
    // Release any resources held by the mx_device_t and free() it.
    // release is called after a device is remove()'d and its
    // refcount hits zero (all closes and unbinds complete)
} mx_protocol_device_t;

// Device Convenience Wrappers
static inline mx_status_t device_get_protocol(mx_device_t* dev, uint32_t proto_id, void** protocol) {
    return dev->ops->get_protocol(dev, proto_id, protocol);
}

// State change functions
// Used by driver to indicate if there's data available to read,
// or room to write, or an error condition.
#define DEV_STATE_READABLE MX_SIGNAL_USER0
#define DEV_STATE_WRITABLE MX_SIGNAL_USER1
#define DEV_STATE_ERROR MX_SIGNAL_USER2

static inline void device_state_set(mx_device_t* dev, mx_signals_t stateflag) {
    _magenta_object_signal(dev->event, stateflag, 0);
}

static inline void device_state_clr(mx_device_t* dev, mx_signals_t stateflag) {
    _magenta_object_signal(dev->event, 0, stateflag);
}

static inline void device_state_set_clr(mx_device_t* dev, mx_signals_t setflag, mx_signals_t clearflag) {
    _magenta_object_signal(dev->event, setflag, clearflag);
}

// Devices which implement just the device protocol and one
// additional protocol may use this common implementation:
mx_status_t device_base_get_protocol(mx_device_t* dev, uint32_t proto_id, void** proto);
