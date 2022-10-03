/** @file
 * MSI - Message signalled interrupts support.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualPox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VPOX_INCLUDED_msi_h
#define VPOX_INCLUDED_msi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/cdefs.h>
#include <VPox/types.h>
#include <iprt/assert.h>

#include <VPox/pci.h>

/* Constants for Intel APIC MSI messages */
#define VPOX_MSI_DATA_VECTOR_SHIFT           0
#define VPOX_MSI_DATA_VECTOR_MASK            0x000000ff
#define VPOX_MSI_DATA_VECTOR(v)              (((v) << VPOX_MSI_DATA_VECTOR_SHIFT) & \
                                                  VPOX_MSI_DATA_VECTOR_MASK)
#define VPOX_MSI_DATA_DELIVERY_MODE_SHIFT    8
#define  VPOX_MSI_DATA_DELIVERY_FIXED        (0 << VPOX_MSI_DATA_DELIVERY_MODE_SHIFT)
#define  VPOX_MSI_DATA_DELIVERY_LOWPRI       (1 << VPOX_MSI_DATA_DELIVERY_MODE_SHIFT)

#define VPOX_MSI_DATA_LEVEL_SHIFT            14
#define  VPOX_MSI_DATA_LEVEL_DEASSERT        (0 << VPOX_MSI_DATA_LEVEL_SHIFT)
#define  VPOX_MSI_DATA_LEVEL_ASSERT          (1 << VPOX_MSI_DATA_LEVEL_SHIFT)

#define VPOX_MSI_DATA_TRIGGER_SHIFT          15
#define  VPOX_MSI_DATA_TRIGGER_EDGE          (0 << VPOX_MSI_DATA_TRIGGER_SHIFT)
#define  VPOX_MSI_DATA_TRIGGER_LEVEL         (1 << VPOX_MSI_DATA_TRIGGER_SHIFT)

/**
 * MSI region, actually same as LAPIC MMIO region, but listens on bus,
 * not CPU, accesses.
 */
#define VPOX_MSI_ADDR_BASE                   0xfee00000
#define VPOX_MSI_ADDR_SIZE                   0x100000

#define VPOX_MSI_ADDR_DEST_MODE_SHIFT        2
#define  VPOX_MSI_ADDR_DEST_MODE_PHYSICAL    (0 << VPOX_MSI_ADDR_DEST_MODE_SHIFT)
#define  VPOX_MSI_ADDR_DEST_MODE_LOGICAL     (1 << VPOX_MSI_ADDR_DEST_MODE_SHIFT)

#define VPOX_MSI_ADDR_REDIRECTION_SHIFT      3
#define  VPOX_MSI_ADDR_REDIRECTION_CPU       (0 << VPOX_MSI_ADDR_REDIRECTION_SHIFT)
                                        /* dedicated cpu */
#define  VPOX_MSI_ADDR_REDIRECTION_LOWPRI    (1 << VPOX_MSI_ADDR_REDIRECTION_SHIFT)
                                        /* lowest priority */

#define VPOX_MSI_ADDR_DEST_ID_SHIFT          12
#define  VPOX_MSI_ADDR_DEST_ID_MASK          0x00ffff0
#define  VPOX_MSI_ADDR_DEST_ID(dest)         (((dest) << VPOX_MSI_ADDR_DEST_ID_SHIFT) & \
                                         VPOX_MSI_ADDR_DEST_ID_MASK)
#define VPOX_MSI_ADDR_EXT_DEST_ID(dest)      ((dest) & 0xffffff00)

#define VPOX_MSI_ADDR_IR_EXT_INT             (1 << 4)
#define VPOX_MSI_ADDR_IR_SHV                 (1 << 3)
#define VPOX_MSI_ADDR_IR_INDEX1(index)       ((index & 0x8000) >> 13)
#define VPOX_MSI_ADDR_IR_INDEX2(index)       ((index & 0x7fff) << 5)

/* Maximum number of vectors, per device/function */
#define VPOX_MSI_MAX_ENTRIES                  32

/* Offsets in MSI PCI capability structure (VPOX_PCI_CAP_ID_MSI) */
#define VPOX_MSI_CAP_MESSAGE_CONTROL          0x02
#define VPOX_MSI_CAP_MESSAGE_ADDRESS_32       0x04
#define VPOX_MSI_CAP_MESSAGE_ADDRESS_LO       0x04
#define VPOX_MSI_CAP_MESSAGE_ADDRESS_HI       0x08
#define VPOX_MSI_CAP_MESSAGE_DATA_32          0x08
#define VPOX_MSI_CAP_MESSAGE_DATA_64          0x0c
#define VPOX_MSI_CAP_MASK_BITS_32             0x0c
#define VPOX_MSI_CAP_PENDING_BITS_32          0x10
#define VPOX_MSI_CAP_MASK_BITS_64             0x10
#define VPOX_MSI_CAP_PENDING_BITS_64          0x14

/* We implement MSI with per-vector masking */
#define VPOX_MSI_CAP_SIZE_32                  0x14
#define VPOX_MSI_CAP_SIZE_64                  0x18

/**
 * MSI-X differs from MSI by the fact that a dedicated physical page (in device
 * memory) is assigned for MSI-X table, and Pending Bit Array (PBA), which is
 * recommended to be separated from the main table by at least 2K.
 *
 * @{
 */
/** Size of a MSI-X page */
#define VPOX_MSIX_PAGE_SIZE                   0x1000
/** Pending interrupts (PBA) */
#define VPOX_MSIX_PAGE_PENDING                (VPOX_MSIX_PAGE_SIZE / 2)
/** Maximum number of vectors, per device/function */
#define VPOX_MSIX_MAX_ENTRIES                 2048
/** Size of MSI-X PCI capability */
#define VPOX_MSIX_CAP_SIZE                    12
/** Offsets in MSI-X PCI capability structure (VPOX_PCI_CAP_ID_MSIX) */
#define VPOX_MSIX_CAP_MESSAGE_CONTROL         0x02
#define VPOX_MSIX_TABLE_BIROFFSET             0x04
#define VPOX_MSIX_PBA_BIROFFSET               0x08
/** Size of single MSI-X table entry */
#define VPOX_MSIX_ENTRY_SIZE                  16
/** @} */


#endif /* !VPOX_INCLUDED_msi_h */
