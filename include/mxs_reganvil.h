/* RegAnvil - Hardware register behavior. Plain C tests.
 * Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT
 */
#ifndef MXS_REGANVIL_H
#define MXS_REGANVIL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MXS_REGANVIL_VERSION_MAJOR 0
#define MXS_REGANVIL_VERSION_MINOR 1
#define MXS_REGANVIL_VERSION_PATCH 0
#define MXS_REGANVIL_VERSION "0.1.0"

/** Recoverable results. No function prints, allocates, or terminates the host. */
typedef enum mxs_reganvil_status {
    MXS_REGANVIL_OK = 0,
    MXS_REGANVIL_INVALID_ARGUMENT,
    MXS_REGANVIL_INSUFFICIENT_CAPACITY,
    MXS_REGANVIL_UNALIGNED_OFFSET,
    MXS_REGANVIL_DUPLICATE_OFFSET,
    MXS_REGANVIL_INVALID_MASK,
    MXS_REGANVIL_INVALID_RESET_VALUE,
    MXS_REGANVIL_UNKNOWN_OFFSET,
    MXS_REGANVIL_OUT_OF_RANGE
} mxs_reganvil_status;

/** One 32-bit register; offsets are byte offsets, unique and four-byte aligned.
 * rw_mask, ro_mask and w1c_mask must be pairwise disjoint. Their union is the
 * implemented mask. All implemented bits are readable; reserved bits read zero.
 * read_clear_mask must be a subset of that union and may overlap any field.
 * reset_value must contain no reserved bits. Unsorted descriptors are valid.
 */
typedef struct mxs_reganvil_descriptor {
    uint32_t offset;
    uint32_t reset_value;
    uint32_t rw_mask;
    uint32_t ro_mask;
    uint32_t w1c_mask;
    uint32_t read_clear_mask;
} mxs_reganvil_descriptor;

typedef enum mxs_reganvil_operation {
    MXS_REGANVIL_READ = 0,
    MXS_REGANVIL_WRITE
} mxs_reganvil_operation;

/** A successful software access. value is the value returned by a read or the
 * entire submitted write value (including ignored bits). before and after are
 * the stored register values on either side of the operation's side effects.
 */
typedef struct mxs_reganvil_trace_record {
    mxs_reganvil_operation operation;
    uint32_t offset;
    uint32_t value;
    uint32_t before;
    uint32_t after;
} mxs_reganvil_trace_record;

/** Counts and capacity in records/events, not bytes. dropped_count saturates at
 * SIZE_MAX; when capacity is zero, tracing is disabled and no drops accumulate.
 */
typedef struct mxs_reganvil_trace_info {
    size_t recorded_count;
    size_t dropped_count;
    size_t capacity;
} mxs_reganvil_trace_info;

/** Caller-owned model. Fields are exposed only to permit allocation without a
 * heap; treat them as private and do not modify them after initialization.
 * Register lookup is O(register_count), with descriptors searched in caller
 * order. Each operation uses constant temporary storage; there is no hidden
 * lookup allocation or descriptor copy.
 *
 * OWNERSHIP / PRECONDITIONS FOR ALL OPERATIONS:
 * - All memory belongs to the caller. No memory is allocated or freed here.
 * - Descriptors are borrowed and must stay valid and immutable for the model's
 *   lifetime. Context, state, and optional trace storage must also stay valid.
 * - These storage regions (including descriptors) must not overlap. Output
 *   arguments must refer to separate valid writable objects, not model storage
 *   or descriptors. Counts must describe the actual accessible array sizes.
 * - Except init and status_string, operations require a successfully initialized
 *   context. Passing arbitrary/uninitialized nonnull memory is not supported.
 * - Operations on a model are single-threaded and non-reentrant. Synchronization
 *   is the caller's responsibility. Independent contexts share no mutable state.
 * - Null required pointers are diagnosed. Dangling pointers, invalid memory
 *   regions, storage overlap, and direct corruption of context/state are not.
 */
typedef struct mxs_reganvil_context {
    const mxs_reganvil_descriptor *descriptors;
    size_t register_count;
    uint32_t *state;
    mxs_reganvil_trace_record *trace;
    size_t trace_capacity;
    size_t trace_count;
    size_t trace_dropped;
} mxs_reganvil_context;

/** Validate the entire configuration, then initialize state with reset values.
 * Duplicate detection takes O(register_count squared) time; no scratch array is
 * required. Initialization is intended for setup rather than the access path.
 * ctx, descriptors and state are required. register_count must be nonzero.
 * state_capacity is in uint32_t elements and must be >= register_count.
 * trace_capacity is in trace_record elements; trace may be NULL only if capacity
 * is zero. Zero capacity disables tracing, even when trace is nonnull.
 * Excess state elements and trace record storage are left untouched. Trace
 * metadata starts empty. All provided storage remains owned by the caller.
 *
 * Returns OK; INVALID_ARGUMENT for null required pointers, zero count, or NULL
 * trace with nonzero capacity; INSUFFICIENT_CAPACITY; UNALIGNED_OFFSET;
 * DUPLICATE_OFFSET; INVALID_MASK (permission overlap or invalid read-clear);
 * or INVALID_RESET_VALUE. Every failure leaves ctx and state entirely unchanged,
 * including when reinitializing an existing model. trace storage is not written.
 */
mxs_reganvil_status mxs_reganvil_init(
    mxs_reganvil_context *ctx,
    const mxs_reganvil_descriptor *descriptors, size_t register_count,
    uint32_t *state, size_t state_capacity,
    mxs_reganvil_trace_record *trace, size_t trace_capacity);

/** Restore all reset values and clear trace counts, including drops. Existing
 * trace storage bytes are not erased. Returns OK or INVALID_ARGUMENT (NULL ctx).
 */
mxs_reganvil_status mxs_reganvil_reset(mxs_reganvil_context *ctx);

/** Return the pre-side-effect value through required out_value, then clear the
 * register's read_clear_mask bits and trace the access. Returns OK,
 * INVALID_ARGUMENT, UNALIGNED_OFFSET, or UNKNOWN_OFFSET. On failure, output,
 * register state, and trace history are unchanged. A full trace is not an error.
 */
mxs_reganvil_status mxs_reganvil_read(
    mxs_reganvil_context *ctx, uint32_t offset, uint32_t *out_value);

/** Software write: replace RW bits, clear W1C bits where value contains ones,
 * preserve RO bits, and ignore reserved bits. A zero leaves W1C bits unchanged.
 * Traces the successful access. Returns OK, INVALID_ARGUMENT, UNALIGNED_OFFSET,
 * or UNKNOWN_OFFSET. Failure changes no state or trace; trace overflow succeeds.
 */
mxs_reganvil_status mxs_reganvil_write(
    mxs_reganvil_context *ctx, uint32_t offset, uint32_t value);

/** Inspect stored value without read-clear or trace side effects. out_value is
 * required and stays unchanged on failure. Same return codes as read.
 */
mxs_reganvil_status mxs_reganvil_peek(
    const mxs_reganvil_context *ctx, uint32_t offset, uint32_t *out_value);

/** Simulated hardware update: (old & ~clear_bits) | set_bits, independent of
 * software permissions (hardware may set RO or W1C bits). Masks must be disjoint
 * subsets of implemented bits. No software semantics or trace event is applied.
 * Returns OK, INVALID_ARGUMENT, UNALIGNED_OFFSET, UNKNOWN_OFFSET, or INVALID_MASK.
 * Every failure leaves state and trace unchanged.
 */
mxs_reganvil_status mxs_reganvil_hardware_update(
    mxs_reganvil_context *ctx, uint32_t offset,
    uint32_t set_bits, uint32_t clear_bits);

/** Copy trace metadata into required out_info. Returns OK or INVALID_ARGUMENT;
 * failure leaves output unchanged. Does not change any model state.
 */
mxs_reganvil_status mxs_reganvil_trace_get_info(
    const mxs_reganvil_context *ctx, mxs_reganvil_trace_info *out_info);

/** Copy a recorded event, indexed from zero in access order, to required
 * out_record. Returns OK, INVALID_ARGUMENT, or OUT_OF_RANGE when index is not
 * below recorded_count (including disabled/empty traces). Failure preserves
 * output. Copied records belong to the caller and survive reset/trace clearing.
 */
mxs_reganvil_status mxs_reganvil_trace_get(
    const mxs_reganvil_context *ctx, size_t index,
    mxs_reganvil_trace_record *out_record);

/** Clear recorded and dropped counts without changing registers or erasing
 * trace storage bytes. New events start at index zero and overwrite old bytes.
 * Returns OK or INVALID_ARGUMENT for NULL ctx.
 */
mxs_reganvil_status mxs_reganvil_trace_clear(mxs_reganvil_context *ctx);

/** Return a nonnull immutable static string for any status, including unknown
 * enum values. The caller must not modify or free it. Requires no model.
 */
const char *mxs_reganvil_status_string(mxs_reganvil_status status);

#ifdef __cplusplus
}
#endif

#endif /* MXS_REGANVIL_H */
