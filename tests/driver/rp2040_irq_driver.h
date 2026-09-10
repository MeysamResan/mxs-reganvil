/* Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT */
#ifndef MXS_REGANVIL_RP2040_IRQ_DRIVER_H
#define MXS_REGANVIL_RP2040_IRQ_DRIVER_H

#include <stdint.h>

/* Original, deliberately small driver for the GPIO0-7 register group only.
 * Byte offsets are relative to IO_BANK0; callbacks perform 32-bit accesses.
 * See README.md here for the datasheet and modeled scope.
 */
#define MXS_REGANVIL_RP2040_INTR0 UINT32_C(0x0f0)
#define MXS_REGANVIL_RP2040_PROC0_INTE0 UINT32_C(0x100)
#define MXS_REGANVIL_RP2040_LEVEL_LOW UINT32_C(1)
#define MXS_REGANVIL_RP2040_LEVEL_HIGH UINT32_C(2)
#define MXS_REGANVIL_RP2040_EDGE_LOW UINT32_C(4)
#define MXS_REGANVIL_RP2040_EDGE_HIGH UINT32_C(8)
#define MXS_REGANVIL_RP2040_EDGE_MASK UINT32_C(0xcccccccc)
#define MXS_REGANVIL_RP2040_LEVEL_MASK UINT32_C(0x33333333)
#define MXS_REGANVIL_RP2040_OK 0
#define MXS_REGANVIL_RP2040_INVALID_ARGUMENT (-1)

typedef int (*mxs_reganvil_rp2040_read_fn)(
    void *context, uint32_t offset, uint32_t *out_value);
typedef int (*mxs_reganvil_rp2040_write_fn)(
    void *context, uint32_t offset, uint32_t value);

/* The callback contract is zero for success, any nonzero value for failure.
 * Failures are propagated unchanged. A callback failure may already have had
 * device side effects; this driver cannot roll those effects back. The context
 * is opaque and may be NULL if the callbacks support that. Calls are serial;
 * enable changes need exclusive access across their read/modify/write sequence.
 */
typedef struct mxs_reganvil_rp2040_irq_driver {
    void *context;
    mxs_reganvil_rp2040_read_fn read;
    mxs_reganvil_rp2040_write_fn write;
} mxs_reganvil_rp2040_irq_driver;

/* Pin must be 0..7, events a four-bit mask, enabled exactly zero or one.
 * Replace just the selected enable bits, preserving every other event/pin.
 * A zero event mask succeeds without accessing registers.
 */
int mxs_reganvil_rp2040_irq_enable(
    const mxs_reganvil_rp2040_irq_driver *driver,
    unsigned int pin, uint32_t events, int enabled);

/* Read the four raw event bits for pin 0..7, regardless of enable state.
 * Required output remains unchanged on all failures.
 */
int mxs_reganvil_rp2040_irq_raw(
    const mxs_reganvil_rp2040_irq_driver *driver,
    unsigned int pin, uint32_t *out_events);

/* Acknowledge selected edge events for pin 0..7 with one direct W1C write.
 * Only EDGE_LOW/EDGE_HIGH are valid. Zero succeeds without an access.
 */
int mxs_reganvil_rp2040_irq_ack(
    const mxs_reganvil_rp2040_irq_driver *driver,
    unsigned int pin, uint32_t edge_events);

/* Read enable then raw state, and acknowledge their intersection with the
 * requested group-wide EDGE_MASK subset. This returns the acknowledged snapshot
 * through required out_acknowledged, preserving it on failure. No pending edges
 * means success with output zero and no write. It is not an atomic snapshot and
 * does not promise to count repeated edges or resolve simultaneous hardware
 * events. The test supplies events between completed driver calls.
 */
int mxs_reganvil_rp2040_irq_take_enabled_edges(
    const mxs_reganvil_rp2040_irq_driver *driver,
    uint32_t requested, uint32_t *out_acknowledged);

#endif
