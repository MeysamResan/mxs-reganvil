/* Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT */
#include "interrupt_driver.h"

#include <stddef.h>

int mxs_reganvil_interrupt_ack_buggy(
    const mxs_reganvil_interrupt_driver *driver, uint32_t flags)
{
    uint32_t status;
    int result;

    if (driver == NULL || driver->read == NULL || driver->write == NULL) {
        return MXS_REGANVIL_DRIVER_INVALID_ARGUMENT;
    }

    result = driver->read(driver->context,
                          MXS_REGANVIL_INTERRUPT_STATUS_OFFSET, &status);
    if (result != MXS_REGANVIL_DRIVER_OK) {
        return result;
    }

    /* W1C status bits already set in status will also be cleared here. */
    return driver->write(driver->context,
                         MXS_REGANVIL_INTERRUPT_STATUS_OFFSET, status | flags);
}

int mxs_reganvil_interrupt_ack(
    const mxs_reganvil_interrupt_driver *driver, uint32_t flags)
{
    if (driver == NULL || driver->write == NULL) {
        return MXS_REGANVIL_DRIVER_INVALID_ARGUMENT;
    }

    return driver->write(driver->context,
                         MXS_REGANVIL_INTERRUPT_STATUS_OFFSET, flags);
}
