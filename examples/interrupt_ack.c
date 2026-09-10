/* Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT */
#include "mxs_reganvil.h"
#include "interrupt_driver.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* These checks intentionally remain active when NDEBUG is defined. */
#define MXS_REGANVIL_DEMO_CHECK(condition)                                   \
    do {                                                                  \
        if (!(condition)) {                                               \
            fprintf(stderr, "RegAnvil: check failed at %s:%d: %s\n",      \
                    __FILE__, __LINE__, #condition);                      \
            return EXIT_FAILURE;                                          \
        }                                                                 \
    } while (0)

static int mxs_reganvil_demo_read(void *context, uint32_t offset,
                                 uint32_t *value)
{
    return (int)mxs_reganvil_read((mxs_reganvil_context *)context, offset,
                                 value);
}

static int mxs_reganvil_demo_write(void *context, uint32_t offset,
                                  uint32_t value)
{
    return (int)mxs_reganvil_write((mxs_reganvil_context *)context, offset,
                                  value);
}

static int mxs_reganvil_demo_print_trace(mxs_reganvil_context *model)
{
    mxs_reganvil_trace_info info;
    size_t index;

    MXS_REGANVIL_DEMO_CHECK(
        mxs_reganvil_trace_get_info(model, &info) == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(info.dropped_count == 0);
    for (index = 0; index < info.recorded_count; ++index) {
        mxs_reganvil_trace_record record;
        MXS_REGANVIL_DEMO_CHECK(
            mxs_reganvil_trace_get(model, index, &record) == MXS_REGANVIL_OK);
        printf("    %-5s @0x%08" PRIx32 " value=0x%08" PRIx32
               " before=0x%08" PRIx32 " after=0x%08" PRIx32 "\n",
               record.operation == MXS_REGANVIL_READ ? "READ" : "WRITE",
               record.offset, record.value, record.before, record.after);
    }
    return EXIT_SUCCESS;
}

int main(void)
{
    const mxs_reganvil_descriptor descriptors[] = {
        {MXS_REGANVIL_INTERRUPT_STATUS_OFFSET, UINT32_C(0x03),
         UINT32_C(0), UINT32_C(0), UINT32_C(0x03), UINT32_C(0)}
    };
    mxs_reganvil_context model;
    uint32_t state[1];
    mxs_reganvil_trace_record trace[4];
    mxs_reganvil_trace_info info;
    mxs_reganvil_trace_record record;
    mxs_reganvil_interrupt_driver driver;
    uint32_t pending;
    mxs_reganvil_status init_status;

    init_status = mxs_reganvil_init(&model, descriptors, 1, state, 1, trace, 4);
    if (init_status != MXS_REGANVIL_OK) {
        fprintf(stderr, "RegAnvil: initialization failed: %s\n",
                mxs_reganvil_status_string(init_status));
        return EXIT_FAILURE;
    }
    driver.context = &model;
    driver.read = mxs_reganvil_demo_read;
    driver.write = mxs_reganvil_demo_write;

    puts("RegAnvil: W1C interrupt acknowledgement");
    MXS_REGANVIL_DEMO_CHECK(mxs_reganvil_peek(
        &model, MXS_REGANVIL_INTERRUPT_STATUS_OFFSET, &pending)
        == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(pending == UINT32_C(0x03));
    printf("  Pending flags at reset: 0x%08" PRIx32 "\n", pending);

    MXS_REGANVIL_DEMO_CHECK(mxs_reganvil_interrupt_ack_buggy(
        &driver, MXS_REGANVIL_INTERRUPT_FIRST_FLAG) == MXS_REGANVIL_DRIVER_OK);
    MXS_REGANVIL_DEMO_CHECK(mxs_reganvil_peek(
        &model, MXS_REGANVIL_INTERRUPT_STATUS_OFFSET, &pending)
        == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(pending == UINT32_C(0));
    MXS_REGANVIL_DEMO_CHECK(pending != MXS_REGANVIL_INTERRUPT_SECOND_FLAG);
    MXS_REGANVIL_DEMO_CHECK(
        mxs_reganvil_trace_get_info(&model, &info) == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(info.recorded_count == 2);
    MXS_REGANVIL_DEMO_CHECK(
        mxs_reganvil_trace_get(&model, 0, &record) == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(record.operation == MXS_REGANVIL_READ);
    MXS_REGANVIL_DEMO_CHECK(record.value == UINT32_C(0x03));
    MXS_REGANVIL_DEMO_CHECK(
        mxs_reganvil_trace_get(&model, 1, &record) == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(record.operation == MXS_REGANVIL_WRITE);
    MXS_REGANVIL_DEMO_CHECK(record.value == UINT32_C(0x03));
    MXS_REGANVIL_DEMO_CHECK(record.before == UINT32_C(0x03));
    MXS_REGANVIL_DEMO_CHECK(record.after == UINT32_C(0));
    printf("  Buggy read/modify/write: expected 0x00000002, observed 0x%08"
           PRIx32 " -- detected\n", pending);
    MXS_REGANVIL_DEMO_CHECK(
        mxs_reganvil_demo_print_trace(&model) == EXIT_SUCCESS);

    MXS_REGANVIL_DEMO_CHECK(mxs_reganvil_reset(&model) == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(mxs_reganvil_interrupt_ack(
        &driver, MXS_REGANVIL_INTERRUPT_FIRST_FLAG) == MXS_REGANVIL_DRIVER_OK);
    MXS_REGANVIL_DEMO_CHECK(mxs_reganvil_peek(
        &model, MXS_REGANVIL_INTERRUPT_STATUS_OFFSET, &pending)
        == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(pending == MXS_REGANVIL_INTERRUPT_SECOND_FLAG);
    MXS_REGANVIL_DEMO_CHECK(
        mxs_reganvil_trace_get_info(&model, &info) == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(info.recorded_count == 1);
    MXS_REGANVIL_DEMO_CHECK(
        mxs_reganvil_trace_get(&model, 0, &record) == MXS_REGANVIL_OK);
    MXS_REGANVIL_DEMO_CHECK(record.operation == MXS_REGANVIL_WRITE);
    MXS_REGANVIL_DEMO_CHECK(record.value == MXS_REGANVIL_INTERRUPT_FIRST_FLAG);
    MXS_REGANVIL_DEMO_CHECK(record.before == UINT32_C(0x03));
    MXS_REGANVIL_DEMO_CHECK(record.after == MXS_REGANVIL_INTERRUPT_SECOND_FLAG);
    printf("  Correct acknowledgement: second flag remains 0x%08"
           PRIx32 " -- passed\n", pending);
    MXS_REGANVIL_DEMO_CHECK(
        mxs_reganvil_demo_print_trace(&model) == EXIT_SUCCESS);
    puts("  PASS: buggy acknowledgement detected; correct acknowledgement passed.");
    return EXIT_SUCCESS;
}
