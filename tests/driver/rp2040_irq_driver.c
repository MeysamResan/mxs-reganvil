/* Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT */
#include "rp2040_irq_driver.h"

#include <stddef.h>

int mxs_reganvil_rp2040_irq_enable(
    const mxs_reganvil_rp2040_irq_driver *driver,
    unsigned int pin, uint32_t events, int enabled)
{
    uint32_t current;
    uint32_t mask;
    int result;
    if (driver == NULL || driver->read == NULL || driver->write == NULL ||
        pin > 7U || (events & ~UINT32_C(15)) != 0 ||
        (enabled != 0 && enabled != 1)) {
        return MXS_REGANVIL_RP2040_INVALID_ARGUMENT;
    }
    if (events == 0) {
        return MXS_REGANVIL_RP2040_OK;
    }
    mask = events << (pin * 4U);
    result = driver->read(driver->context,
        MXS_REGANVIL_RP2040_PROC0_INTE0, &current);
    if (result != MXS_REGANVIL_RP2040_OK) {
        return result;
    }
    current = enabled != 0 ? current | mask : current & ~mask;
    return driver->write(driver->context,
        MXS_REGANVIL_RP2040_PROC0_INTE0, current);
}

int mxs_reganvil_rp2040_irq_raw(
    const mxs_reganvil_rp2040_irq_driver *driver,
    unsigned int pin, uint32_t *out_events)
{
    uint32_t raw;
    int result;
    if (driver == NULL || driver->read == NULL || pin > 7U ||
        out_events == NULL) {
        return MXS_REGANVIL_RP2040_INVALID_ARGUMENT;
    }
    result = driver->read(driver->context, MXS_REGANVIL_RP2040_INTR0, &raw);
    if (result != MXS_REGANVIL_RP2040_OK) {
        return result;
    }
    *out_events = (raw >> (pin * 4U)) & UINT32_C(15);
    return MXS_REGANVIL_RP2040_OK;
}

int mxs_reganvil_rp2040_irq_ack(
    const mxs_reganvil_rp2040_irq_driver *driver,
    unsigned int pin, uint32_t edge_events)
{
    if (driver == NULL || driver->write == NULL || pin > 7U ||
        (edge_events & ~UINT32_C(12)) != 0) {
        return MXS_REGANVIL_RP2040_INVALID_ARGUMENT;
    }
    if (edge_events == 0) {
        return MXS_REGANVIL_RP2040_OK;
    }
    return driver->write(driver->context, MXS_REGANVIL_RP2040_INTR0,
        edge_events << (pin * 4U));
}

int mxs_reganvil_rp2040_irq_take_enabled_edges(
    const mxs_reganvil_rp2040_irq_driver *driver,
    uint32_t requested, uint32_t *out_acknowledged)
{
    uint32_t enabled;
    uint32_t raw;
    uint32_t pending;
    int result;
    if (driver == NULL || driver->read == NULL || driver->write == NULL ||
        out_acknowledged == NULL ||
        (requested & ~MXS_REGANVIL_RP2040_EDGE_MASK) != 0) {
        return MXS_REGANVIL_RP2040_INVALID_ARGUMENT;
    }
    if (requested == 0) {
        *out_acknowledged = 0;
        return MXS_REGANVIL_RP2040_OK;
    }
    result = driver->read(driver->context,
        MXS_REGANVIL_RP2040_PROC0_INTE0, &enabled);
    if (result != MXS_REGANVIL_RP2040_OK) {
        return result;
    }
    result = driver->read(driver->context, MXS_REGANVIL_RP2040_INTR0, &raw);
    if (result != MXS_REGANVIL_RP2040_OK) {
        return result;
    }
    pending = requested & enabled & raw;
    if (pending != 0) {
        result = driver->write(driver->context, MXS_REGANVIL_RP2040_INTR0,
            pending);
        if (result != MXS_REGANVIL_RP2040_OK) {
            return result;
        }
    }
    *out_acknowledged = pending;
    return MXS_REGANVIL_RP2040_OK;
}
