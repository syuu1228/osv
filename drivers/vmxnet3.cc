/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */


#define _KERNEL

#include <sys/cdefs.h>

#include "drivers/vmware.hh"
#include "drivers/vmxnet3.hh"
#include "drivers/pci-device.hh"
#include "interrupt.hh"

#include "mempool.hh"
#include "mmu.hh"

#include <sstream>
#include <string>
#include <string.h>
#include <map>
#include <errno.h>
#include <osv/debug.h>

#include "sched.hh"
#include "osv/trace.hh"

#include "drivers/clock.hh"
#include "drivers/clockevent.hh"

#include <osv/device.h>
#include <osv/ioctl.h>
#include <bsd/sys/net/ethernet.h>
#include <bsd/sys/net/if_types.h>
#include <bsd/sys/sys/param.h>

#include <bsd/sys/net/ethernet.h>
#include <bsd/sys/net/if_vlan_var.h>
#include <bsd/sys/netinet/in.h>
#include <bsd/sys/netinet/ip.h>
#include <bsd/sys/netinet/udp.h>
#include <bsd/sys/netinet/tcp.h>

using namespace memory;

namespace vmware {

    int vmxnet3::_instance = 0;

    #define vmxnet3_tag "vmxnet3"
    #define vmxnet3_d(...)   tprintf_d(vmxnet3_tag, __VA_ARGS__)
    #define vmxnet3_i(...)   tprintf_i(vmxnet3_tag, __VA_ARGS__)
    #define vmxnet3_w(...)   tprintf_w(vmxnet3_tag, __VA_ARGS__)
    #define vmxnet3_e(...)   tprintf_e(vmxnet3_tag, __VA_ARGS__)

    vmxnet3_drv_shared::vmxnet3_drv_shared()
    {
        _layout = static_cast<vmxnet3_shared_layout*>
            (memory::alloc_phys_contiguous_aligned(sizeof(*_layout), 1));

        if(!_layout)
            throw std::runtime_error("VMXNET3 driver shared area allocation failed");

        init_layout();
    }

    vmxnet3_drv_shared::~vmxnet3_drv_shared()
    {
        memory::free_phys_contiguous_aligned(_layout);
    }

    void vmxnet3_drv_shared::init_layout()
    {
        memset(_layout, 0, sizeof(*_layout));

        _layout->magic = VMXNET3_REV1_MAGIC;

        // DriverInfo
        _layout->version = VMXNET3_DRIVER_VERSION;
        _layout->guest = VMXNET3_GOS_FREEBSD | VMXNET3_GUEST_OS_VERSION |
        (sizeof(void*) == sizeof(u32) ? VMXNET3_GOS_32BIT : VMXNET3_GOS_32BIT);

        _layout->vmxnet3_revision = VMXNET3_REVISION;
        _layout->upt_version = VMXNET3_UPT_VERSION;
    }

    vmxnet3::vmxnet3(pci::device& dev)
        : vmware_driver(dev)
    {
        vmxnet3_i("VMXNET3 INSTANCE");
        _id = _instance++;
        parse_pci_config();
        do_version_handshake();
    }

    hw_driver* vmxnet3::probe(hw_device* dev)
    {
        try {
            if (auto pci_dev = dynamic_cast<pci::device*>(dev)) {
                if (pci_dev->get_id() == hw_device_id(VMWARE_VENDOR_ID, VMXNET3_DEVICE_ID)) {
                    return new vmxnet3(*pci_dev);
                }
            }
        } catch (std::exception& e) {
            vmxnet3_e("Exception on device construction: %s", e.what());
        }
        return nullptr;
    }

    void vmxnet3::parse_pci_config(void)
    {
        _bar1 = _dev.get_bar(1);
        if (_bar1 == nullptr) {
            throw std::runtime_error("BAR1 is absent");
        }

        _bar2 = _dev.get_bar(2);
        if (_bar2 == nullptr) {
            throw std::runtime_error("BAR2 is absent");
        }
    }

    void vmxnet3::do_version_handshake(void)
    {
        auto val = _bar1->read(VMXNET3_BAR1_VRRS);
        if ((val & VMXNET3_VERSIONS_MASK) != VMXNET3_REVISION) {
            auto err = boost::format("unknown HW version %d") % val;
            throw std::runtime_error(err.str());
        }
        _bar1->write(VMXNET3_BAR1_VRRS, VMXNET3_REVISION);

        val = _bar1->read(VMXNET3_BAR1_UVRS);
        if ((val & VMXNET3_VERSIONS_MASK) != VMXNET3_UPT_VERSION) {
            auto err = boost::format("unknown UPT version %d") % val;
            throw std::runtime_error(err.str());
        }
        _bar1->write(VMXNET3_BAR1_UVRS, VMXNET3_UPT_VERSION);
    }

}

