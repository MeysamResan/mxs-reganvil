/* Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT */
#ifndef MXS_REGANVIL_INTERRUPT_DRIVER_H
#define MXS_REGANVIL_INTERRUPT_DRIVER_H

#include <stdint.h>

/*
 * A small driver-facing I/O seam. A production adapter can use actual device
 * accessors; the test adapter uses RegAnvil. Callbacks return zero on success
 * and a nonzero error otherwise. Read callbacks fill value on success.
 * The caller owns the callback context and keeps it alive for each operation.
 */
typedef int (*mxs_reganvil_driver_read_fn)(void *context, uint32_t offset,
                                          uint32_t *value);
typedef int (*mxs_reganvil_driver_write_fn)(void *context, uint32_t offset,
                                           uint32_t value);

typedef struct mxs_reganvil_interrupt_driver {
    void *context;
    mxs_reganvil_driver_read_fn read;
    mxs_reganvil_driver_write_fn write;
} mxs_reganvil_interrupt_driver;

#define MXS_REGANVIL_INTERRUPT_STATUS_OFFSET UINT32_C(0x10)
#define MXS_REGANVIL_INTERRUPT_FIRST_FLAG UINT32_C(0x01)
#define MXS_REGANVIL_INTERRUPT_SECOND_FLAG UINT32_C(0x02)
#define MXS_REGANVIL_DRIVER_OK 0
#define MXS_REGANVIL_DRIVER_INVALID_ARGUMENT (-1)

/* Deliberate W1C bug for the demo: reads status before writing status | flags. */
int mxs_reganvil_interrupt_ack_buggy(
    const mxs_reganvil_interrupt_driver *driver, uint32_t flags);

/* Correct W1C acknowledgement: write only the flags being acknowledged. */
int mxs_reganvil_interrupt_ack(
    const mxs_reganvil_interrupt_driver *driver, uint32_t flags);

#endif
