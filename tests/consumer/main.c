/* Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT */
#include "mxs_reganvil.h"

/* This separate translation unit deliberately needs only the public header. */
int main(void)
{
    const mxs_reganvil_descriptor descriptors[] = {
        { UINT32_C(0), UINT32_C(3), UINT32_C(0), UINT32_C(0),
          UINT32_C(3), UINT32_C(0) }
    };
    mxs_reganvil_context context;
    uint32_t state[1];
    uint32_t value = UINT32_C(0);
    mxs_reganvil_trace_record trace[1];
    mxs_reganvil_trace_record record;
    mxs_reganvil_trace_info info;

    if(mxs_reganvil_init(&context, descriptors, 1, state, 1, trace, 1)
            != MXS_REGANVIL_OK
        || mxs_reganvil_write(&context, UINT32_C(0), UINT32_C(1))
            != MXS_REGANVIL_OK
        || mxs_reganvil_peek(&context, UINT32_C(0), &value)
            != MXS_REGANVIL_OK
        || value != UINT32_C(2)
        || mxs_reganvil_trace_get_info(&context, &info) != MXS_REGANVIL_OK
        || info.recorded_count != 1 || info.dropped_count != 0
        || mxs_reganvil_trace_get(&context, 0, &record) != MXS_REGANVIL_OK
        || record.operation != MXS_REGANVIL_WRITE
        || record.before != UINT32_C(3) || record.after != UINT32_C(2)) {
        return 1;
    }
    return 0;
}
