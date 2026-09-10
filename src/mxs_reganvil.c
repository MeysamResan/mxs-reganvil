/* Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT */
#include "mxs_reganvil.h"

static uint32_t mxs_reganvil_implemented(const mxs_reganvil_descriptor *desc)
{
    return desc->rw_mask | desc->ro_mask | desc->w1c_mask;
}

/* Keep the caller's descriptor order and use no auxiliary index storage.
 * A linear scan is deliberate for the small, possibly sparse/unsorted models.
 */
static mxs_reganvil_status mxs_reganvil_find(
    const mxs_reganvil_context *ctx, uint32_t offset, size_t *index)
{
    size_t i;
    if (ctx == NULL) {
        return MXS_REGANVIL_INVALID_ARGUMENT;
    }
    if ((offset & UINT32_C(3)) != 0) {
        return MXS_REGANVIL_UNALIGNED_OFFSET;
    }
    for (i = 0; i < ctx->register_count; ++i) {
        if (ctx->descriptors[i].offset == offset) {
            *index = i;
            return MXS_REGANVIL_OK;
        }
    }
    return MXS_REGANVIL_UNKNOWN_OFFSET;
}

static void mxs_reganvil_record(mxs_reganvil_context *ctx,
    mxs_reganvil_operation operation, uint32_t offset,
    uint32_t value, uint32_t before, uint32_t after)
{
    mxs_reganvil_trace_record *record;
    if (ctx->trace_capacity == 0) {
        return;
    }
    /* Keep the earliest events when full; overflow never blocks an access. */
    if (ctx->trace_count == ctx->trace_capacity) {
        if (ctx->trace_dropped != SIZE_MAX) {
            ++ctx->trace_dropped;
        }
        return;
    }
    record = &ctx->trace[ctx->trace_count];
    record->operation = operation;
    record->offset = offset;
    record->value = value;
    record->before = before;
    record->after = after;
    ++ctx->trace_count;
}

mxs_reganvil_status mxs_reganvil_init(
    mxs_reganvil_context *ctx,
    const mxs_reganvil_descriptor *descriptors, size_t register_count,
    uint32_t *state, size_t state_capacity,
    mxs_reganvil_trace_record *trace, size_t trace_capacity)
{
    size_t i;
    size_t j;
    if (ctx == NULL || descriptors == NULL || state == NULL ||
        register_count == 0 || (trace == NULL && trace_capacity != 0)) {
        return MXS_REGANVIL_INVALID_ARGUMENT;
    }
    if (state_capacity < register_count) {
        return MXS_REGANVIL_INSUFFICIENT_CAPACITY;
    }
    /* This entire pass must remain read-only: a failed reinitialization must
     * also preserve an existing model, its state, and its trace history.
     */
    for (i = 0; i < register_count; ++i) {
        const mxs_reganvil_descriptor *desc = &descriptors[i];
        uint32_t implemented = mxs_reganvil_implemented(desc);
        if ((desc->offset & UINT32_C(3)) != 0) {
            return MXS_REGANVIL_UNALIGNED_OFFSET;
        }
        for (j = 0; j < i; ++j) {
            if (descriptors[j].offset == desc->offset) {
                return MXS_REGANVIL_DUPLICATE_OFFSET;
            }
        }
        if ((desc->rw_mask & desc->ro_mask) != 0 ||
            (desc->rw_mask & desc->w1c_mask) != 0 ||
            (desc->ro_mask & desc->w1c_mask) != 0 ||
            (desc->read_clear_mask & ~implemented) != 0) {
            return MXS_REGANVIL_INVALID_MASK;
        }
        if ((desc->reset_value & ~implemented) != 0) {
            return MXS_REGANVIL_INVALID_RESET_VALUE;
        }
    }
    ctx->descriptors = descriptors;
    ctx->register_count = register_count;
    ctx->state = state;
    ctx->trace = trace;
    ctx->trace_capacity = trace_capacity;
    ctx->trace_count = 0;
    ctx->trace_dropped = 0;
    for (i = 0; i < register_count; ++i) {
        state[i] = descriptors[i].reset_value;
    }
    return MXS_REGANVIL_OK;
}

mxs_reganvil_status mxs_reganvil_reset(mxs_reganvil_context *ctx)
{
    size_t i;
    if (ctx == NULL) {
        return MXS_REGANVIL_INVALID_ARGUMENT;
    }
    for (i = 0; i < ctx->register_count; ++i) {
        ctx->state[i] = ctx->descriptors[i].reset_value;
    }
    return mxs_reganvil_trace_clear(ctx);
}

mxs_reganvil_status mxs_reganvil_read(
    mxs_reganvil_context *ctx, uint32_t offset, uint32_t *out_value)
{
    size_t index;
    uint32_t before;
    mxs_reganvil_status status;
    if (out_value == NULL) {
        return MXS_REGANVIL_INVALID_ARGUMENT;
    }
    status = mxs_reganvil_find(ctx, offset, &index);
    if (status != MXS_REGANVIL_OK) {
        return status;
    }
    /* The caller receives the snapshot; the trace also captures the cleared state. */
    before = ctx->state[index];
    ctx->state[index] = before & ~ctx->descriptors[index].read_clear_mask;
    *out_value = before;
    mxs_reganvil_record(ctx, MXS_REGANVIL_READ, offset, before,
        before, ctx->state[index]);
    return MXS_REGANVIL_OK;
}

mxs_reganvil_status mxs_reganvil_write(
    mxs_reganvil_context *ctx, uint32_t offset, uint32_t value)
{
    size_t index;
    uint32_t before;
    uint32_t after;
    const mxs_reganvil_descriptor *desc;
    mxs_reganvil_status status = mxs_reganvil_find(ctx, offset, &index);
    if (status != MXS_REGANVIL_OK) {
        return status;
    }
    desc = &ctx->descriptors[index];
    before = ctx->state[index];
    /* Init and hardware updates keep reserved bits zero. RW replacement cannot
     * introduce them, and the disjoint W1C mask cannot clear an RW bit.
     */
    after = (before & ~desc->rw_mask) | (value & desc->rw_mask);
    after &= ~(value & desc->w1c_mask);
    ctx->state[index] = after;
    mxs_reganvil_record(ctx, MXS_REGANVIL_WRITE, offset, value, before, after);
    return MXS_REGANVIL_OK;
}

mxs_reganvil_status mxs_reganvil_peek(
    const mxs_reganvil_context *ctx, uint32_t offset, uint32_t *out_value)
{
    size_t index;
    mxs_reganvil_status status;
    if (out_value == NULL) {
        return MXS_REGANVIL_INVALID_ARGUMENT;
    }
    status = mxs_reganvil_find(ctx, offset, &index);
    if (status != MXS_REGANVIL_OK) {
        return status;
    }
    *out_value = ctx->state[index];
    return MXS_REGANVIL_OK;
}

mxs_reganvil_status mxs_reganvil_hardware_update(
    mxs_reganvil_context *ctx, uint32_t offset,
    uint32_t set_bits, uint32_t clear_bits)
{
    size_t index;
    uint32_t implemented;
    mxs_reganvil_status status = mxs_reganvil_find(ctx, offset, &index);
    if (status != MXS_REGANVIL_OK) {
        return status;
    }
    implemented = mxs_reganvil_implemented(&ctx->descriptors[index]);
    if ((set_bits & clear_bits) != 0 ||
        ((set_bits | clear_bits) & ~implemented) != 0) {
        return MXS_REGANVIL_INVALID_MASK;
    }
    ctx->state[index] = (ctx->state[index] & ~clear_bits) | set_bits;
    return MXS_REGANVIL_OK;
}

mxs_reganvil_status mxs_reganvil_trace_get_info(
    const mxs_reganvil_context *ctx, mxs_reganvil_trace_info *out_info)
{
    if (ctx == NULL || out_info == NULL) {
        return MXS_REGANVIL_INVALID_ARGUMENT;
    }
    out_info->recorded_count = ctx->trace_count;
    out_info->dropped_count = ctx->trace_dropped;
    out_info->capacity = ctx->trace_capacity;
    return MXS_REGANVIL_OK;
}

mxs_reganvil_status mxs_reganvil_trace_get(
    const mxs_reganvil_context *ctx, size_t index,
    mxs_reganvil_trace_record *out_record)
{
    if (ctx == NULL || out_record == NULL) {
        return MXS_REGANVIL_INVALID_ARGUMENT;
    }
    if (index >= ctx->trace_count) {
        return MXS_REGANVIL_OUT_OF_RANGE;
    }
    *out_record = ctx->trace[index];
    return MXS_REGANVIL_OK;
}

mxs_reganvil_status mxs_reganvil_trace_clear(mxs_reganvil_context *ctx)
{
    if (ctx == NULL) {
        return MXS_REGANVIL_INVALID_ARGUMENT;
    }
    ctx->trace_count = 0;
    ctx->trace_dropped = 0;
    return MXS_REGANVIL_OK;
}

const char *mxs_reganvil_status_string(mxs_reganvil_status status)
{
    switch (status) {
    case MXS_REGANVIL_OK: return "success";
    case MXS_REGANVIL_INVALID_ARGUMENT: return "invalid argument";
    case MXS_REGANVIL_INSUFFICIENT_CAPACITY: return "insufficient state capacity";
    case MXS_REGANVIL_UNALIGNED_OFFSET: return "unaligned register offset";
    case MXS_REGANVIL_DUPLICATE_OFFSET: return "duplicate register offset";
    case MXS_REGANVIL_INVALID_MASK: return "invalid register mask";
    case MXS_REGANVIL_INVALID_RESET_VALUE: return "invalid reset value";
    case MXS_REGANVIL_UNKNOWN_OFFSET: return "unknown register offset";
    case MXS_REGANVIL_OUT_OF_RANGE: return "trace index out of range";
    default: return "unknown status";
    }
}
