/* RegAnvil behavioral tests. Copyright (c) 2026 RegAnvil contributors.
 * SPDX-License-Identifier: MIT
 */
#include "mxs_reganvil.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Checks deliberately remain active when NDEBUG is defined. */
static size_t checks_run;
static size_t failures;
static int randomized_check;
static uint32_t random_seed;
static size_t random_operation;

static void failure_location(int line)
{
    fprintf(stderr, "test_reganvil.c:%d", line);
    if (randomized_check) {
        fprintf(stderr, " [seed=0x%08" PRIx32 ", operation=%zu]",
                random_seed, random_operation);
    }
}

static int check_condition(int condition, const char *expression, int line)
{
    ++checks_run;
    if (!condition) {
        ++failures;
        failure_location(line);
        fprintf(stderr, ": check failed: %s\n", expression);
    }
    return condition;
}

static int check_status(mxs_reganvil_status actual,
                        mxs_reganvil_status expected, int line)
{
    ++checks_run;
    if (actual != expected) {
        ++failures;
        failure_location(line);
        fprintf(stderr, ": status %d (%s), expected %d (%s)\n",
                (int)actual, mxs_reganvil_status_string(actual),
                (int)expected, mxs_reganvil_status_string(expected));
        return 0;
    }
    return 1;
}

static int check_value(uint32_t actual, uint32_t expected, int line)
{
    ++checks_run;
    if (actual != expected) {
        ++failures;
        failure_location(line);
        fprintf(stderr, ": value 0x%08" PRIx32 ", expected 0x%08" PRIx32 "\n",
                actual, expected);
        return 0;
    }
    return 1;
}

#define CHECK(expression) ((void)check_condition((expression), #expression, __LINE__))
#define STATUS(expression, expected) \
    ((void)check_status((expression), (expected), __LINE__))
#define VALUE(actual, expected) \
    ((void)check_value((actual), (expected), __LINE__))
#define REQUIRE_OK(expression) \
    do { \
        if (!check_status((expression), MXS_REGANVIL_OK, __LINE__)) { \
            return; \
        } \
    } while (0)

static void expect_peek(const mxs_reganvil_context *context,
                        uint32_t offset, uint32_t expected)
{
    uint32_t value = UINT32_C(0xdeadbeef);
    STATUS(mxs_reganvil_peek(context, offset, &value), MXS_REGANVIL_OK);
    VALUE(value, expected);
}

static void expect_trace_info(const mxs_reganvil_context *context,
                              size_t recorded, size_t dropped, size_t capacity)
{
    mxs_reganvil_trace_info info;
    memset(&info, 0, sizeof(info));
    STATUS(mxs_reganvil_trace_get_info(context, &info), MXS_REGANVIL_OK);
    CHECK(info.recorded_count == recorded);
    CHECK(info.dropped_count == dropped);
    CHECK(info.capacity == capacity);
}

static void expect_record(const mxs_reganvil_context *context, size_t index,
                           mxs_reganvil_operation operation, uint32_t offset,
                           uint32_t value, uint32_t before, uint32_t after)
{
    mxs_reganvil_trace_record record;
    memset(&record, 0, sizeof(record));
    STATUS(mxs_reganvil_trace_get(context, index, &record), MXS_REGANVIL_OK);
    CHECK(record.operation == operation);
    VALUE(record.offset, offset);
    VALUE(record.value, value);
    VALUE(record.before, before);
    VALUE(record.after, after);
}

static void test_initialization_and_reset(void)
{
    const mxs_reganvil_descriptor descriptors[] = {
        {8, UINT32_C(0x123456), UINT32_C(0xff), UINT32_C(0xff00),
         UINT32_C(0xff0000), 0},
        {0, UINT32_C(0x80000001), UINT32_MAX, 0, 0, 0},
        {4, UINT32_C(3), 0, 0, UINT32_C(3), UINT32_C(1)}
    };
    struct {
        uint32_t before;
        uint32_t values[4];
        uint32_t after;
    } state = {UINT32_C(0x1234abcd), {0, 0, 0, UINT32_C(0xfedcba98)},
               UINT32_C(0x87654321)};
    struct {
        uint32_t before;
        mxs_reganvil_trace_record records[4];
        uint32_t after;
    } trace;
    mxs_reganvil_trace_record initial_trace[4];
    mxs_reganvil_trace_record recorded_trace[4];
    mxs_reganvil_context context;
    uint32_t value = 0;
    size_t i;

    memset(&trace, 0xa5, sizeof(trace));
    trace.before = UINT32_C(0x01234567);
    trace.after = UINT32_C(0x89abcdef);
    memcpy(initial_trace, trace.records, sizeof(initial_trace));
    REQUIRE_OK(mxs_reganvil_init(&context, descriptors, 3, state.values, 4,
                                trace.records, 4));
    CHECK(memcmp(trace.records, initial_trace, sizeof(initial_trace)) == 0);
    for (i = 0; i < 3; ++i) {
        VALUE(state.values[i], descriptors[i].reset_value);
        expect_peek(&context, descriptors[i].offset, descriptors[i].reset_value);
    }
    expect_trace_info(&context, 0, 0, 4);
    STATUS(mxs_reganvil_write(&context, 8, UINT32_MAX), MXS_REGANVIL_OK);
    STATUS(mxs_reganvil_write(&context, 0, 0), MXS_REGANVIL_OK);
    STATUS(mxs_reganvil_read(&context, 4, &value), MXS_REGANVIL_OK);
    VALUE(value, 3);
    expect_peek(&context, 4, 2);
    expect_trace_info(&context, 3, 0, 4);
    memcpy(recorded_trace, trace.records, sizeof(recorded_trace));
    STATUS(mxs_reganvil_reset(&context), MXS_REGANVIL_OK);
    CHECK(memcmp(trace.records, recorded_trace, sizeof(recorded_trace)) == 0);
    for (i = 0; i < 3; ++i) {
        VALUE(state.values[i], descriptors[i].reset_value);
    }
    expect_trace_info(&context, 0, 0, 4);
    VALUE(state.before, UINT32_C(0x1234abcd));
    VALUE(state.values[3], UINT32_C(0xfedcba98));
    VALUE(state.after, UINT32_C(0x87654321));
    VALUE(trace.before, UINT32_C(0x01234567));
    VALUE(trace.after, UINT32_C(0x89abcdef));
}

static void test_software_semantics(void)
{
    const mxs_reganvil_descriptor descriptors[] = {
        {0, UINT32_C(0x56), UINT32_C(0xff), 0, 0, 0},
        {4, UINT32_C(0xa5), 0, UINT32_C(0xff), 0, 0},
        {8, UINT32_C(3), 0, 0, UINT32_C(3), 0},
        {12, UINT32_C(0x753), UINT32_C(0xf), UINT32_C(0xf0),
         UINT32_C(0xf00), 0},
        {16, UINT32_C(0x3f), UINT32_C(3), UINT32_C(0xc),
         UINT32_C(0x30), UINT32_C(0x15)},
        {20, UINT32_C(0x80000001), UINT32_MAX, 0, 0, 0},
        {24, UINT32_C(0xc0000001), UINT32_C(0x40000000),
         UINT32_C(0x80000000), UINT32_C(1), UINT32_C(0x80000001)},
        {28, 0, 0, 0, 0, 0},
        {32, UINT32_MAX, 0, 0, UINT32_MAX, 0},
        {36, UINT32_MAX, 0, UINT32_MAX, 0, UINT32_MAX}
    };
    mxs_reganvil_context context;
    uint32_t state[10];
    uint32_t value = 0;

    REQUIRE_OK(mxs_reganvil_init(&context, descriptors, 10, state, 10, NULL, 0));
    STATUS(mxs_reganvil_write(&context, 0, UINT32_C(0x12345678)), MXS_REGANVIL_OK);
    STATUS(mxs_reganvil_read(&context, 0, &value), MXS_REGANVIL_OK);
    VALUE(value, UINT32_C(0x78));
    STATUS(mxs_reganvil_write(&context, 0, 0), MXS_REGANVIL_OK);
    expect_peek(&context, 0, 0);
    STATUS(mxs_reganvil_write(&context, 4, 0), MXS_REGANVIL_OK);
    expect_peek(&context, 4, UINT32_C(0xa5));
    STATUS(mxs_reganvil_write(&context, 4, UINT32_MAX), MXS_REGANVIL_OK);
    expect_peek(&context, 4, UINT32_C(0xa5));
    STATUS(mxs_reganvil_write(&context, 8, 0), MXS_REGANVIL_OK);
    expect_peek(&context, 8, 3);
    STATUS(mxs_reganvil_write(&context, 8, 1), MXS_REGANVIL_OK);
    expect_peek(&context, 8, 2);
    STATUS(mxs_reganvil_write(&context, 8, 0), MXS_REGANVIL_OK);
    expect_peek(&context, 8, 2);
    STATUS(mxs_reganvil_write(&context, 12, UINT32_C(0xfffff5f5)), MXS_REGANVIL_OK);
    expect_peek(&context, 12, UINT32_C(0x255));

    /* Read-clear overlaps RW, RO, and W1C in this same register. */
    expect_peek(&context, 16, UINT32_C(0x3f));
    expect_peek(&context, 16, UINT32_C(0x3f));
    STATUS(mxs_reganvil_read(&context, 16, &value), MXS_REGANVIL_OK);
    VALUE(value, UINT32_C(0x3f));
    expect_peek(&context, 16, UINT32_C(0x2a));
    STATUS(mxs_reganvil_read(&context, 16, &value), MXS_REGANVIL_OK);
    VALUE(value, UINT32_C(0x2a));
    STATUS(mxs_reganvil_write(&context, 16, UINT32_C(0x21)), MXS_REGANVIL_OK);
    expect_peek(&context, 16, UINT32_C(9));

    STATUS(mxs_reganvil_write(&context, 20, UINT32_MAX), MXS_REGANVIL_OK);
    expect_peek(&context, 20, UINT32_MAX);
    STATUS(mxs_reganvil_write(&context, 20, UINT32_C(0x80000000)), MXS_REGANVIL_OK);
    expect_peek(&context, 20, UINT32_C(0x80000000));
    STATUS(mxs_reganvil_write(&context, 20, 1), MXS_REGANVIL_OK);
    expect_peek(&context, 20, 1);
    STATUS(mxs_reganvil_read(&context, 24, &value), MXS_REGANVIL_OK);
    VALUE(value, UINT32_C(0xc0000001));
    expect_peek(&context, 24, UINT32_C(0x40000000));
    STATUS(mxs_reganvil_hardware_update(&context, 24, UINT32_C(0x80000001), 0),
           MXS_REGANVIL_OK);
    STATUS(mxs_reganvil_write(&context, 24, UINT32_MAX), MXS_REGANVIL_OK);
    expect_peek(&context, 24, UINT32_C(0xc0000000));
    STATUS(mxs_reganvil_write(&context, 28, UINT32_MAX), MXS_REGANVIL_OK);
    STATUS(mxs_reganvil_read(&context, 28, &value), MXS_REGANVIL_OK);
    VALUE(value, 0);
    STATUS(mxs_reganvil_write(&context, 32, UINT32_C(0x80000001)), MXS_REGANVIL_OK);
    expect_peek(&context, 32, UINT32_C(0x7ffffffe));
    STATUS(mxs_reganvil_write(&context, 32, UINT32_MAX), MXS_REGANVIL_OK);
    expect_peek(&context, 32, 0);
    STATUS(mxs_reganvil_write(&context, 36, 0), MXS_REGANVIL_OK);
    expect_peek(&context, 36, UINT32_MAX);
    STATUS(mxs_reganvil_read(&context, 36, &value), MXS_REGANVIL_OK);
    VALUE(value, UINT32_MAX);
    expect_peek(&context, 36, 0);
    expect_trace_info(&context, 0, 0, 0);
}

static void test_hardware_updates(void)
{
    const mxs_reganvil_descriptor descriptor = {
        0, UINT32_C(0x100), UINT32_C(0xff), UINT32_C(0xff00),
        UINT32_C(0xff0000), UINT32_C(0x800000)
    };
    mxs_reganvil_context context;
    mxs_reganvil_trace_record trace[2];
    uint32_t state;

    REQUIRE_OK(mxs_reganvil_init(&context, &descriptor, 1, &state, 1, trace, 2));
    STATUS(mxs_reganvil_hardware_update(&context, 0, UINT32_C(0x808080),
                                       UINT32_C(0x100)), MXS_REGANVIL_OK);
    expect_peek(&context, 0, UINT32_C(0x808080));
    STATUS(mxs_reganvil_hardware_update(&context, 0, UINT32_C(0x010100),
                                       UINT32_C(0x000080)), MXS_REGANVIL_OK);
    expect_peek(&context, 0, UINT32_C(0x818100));
    STATUS(mxs_reganvil_hardware_update(&context, 0, 0, 0), MXS_REGANVIL_OK);
    expect_peek(&context, 0, UINT32_C(0x818100));
    STATUS(mxs_reganvil_hardware_update(&context, 0, 1, 1), MXS_REGANVIL_INVALID_MASK);
    expect_peek(&context, 0, UINT32_C(0x818100));
    STATUS(mxs_reganvil_hardware_update(&context, 0, UINT32_C(0x80000000), 0),
           MXS_REGANVIL_INVALID_MASK);
    expect_peek(&context, 0, UINT32_C(0x818100));
    STATUS(mxs_reganvil_hardware_update(&context, 0, 0, UINT32_C(0x80000000)),
           MXS_REGANVIL_INVALID_MASK);
    expect_peek(&context, 0, UINT32_C(0x818100));
    expect_trace_info(&context, 0, 0, 2);
    STATUS(mxs_reganvil_hardware_update(&context, 0, 0, UINT32_C(0xffffff)),
           MXS_REGANVIL_OK);
    expect_peek(&context, 0, 0);
}

static void test_initialization_failures(void)
{
    const mxs_reganvil_descriptor valid[] = {
        {12, 1, UINT32_C(3), UINT32_C(0xc), UINT32_C(0x30), 0},
        {0, 2, UINT32_C(3), UINT32_C(0xc), UINT32_C(0x30), UINT32_C(0x15)},
        {UINT32_C(0xfffffffc), 3, UINT32_C(3), UINT32_C(0xc), UINT32_C(0x30), 0}
    };
    const mxs_reganvil_status expected[] = {
        MXS_REGANVIL_INVALID_ARGUMENT, MXS_REGANVIL_INVALID_ARGUMENT,
        MXS_REGANVIL_INVALID_ARGUMENT, MXS_REGANVIL_INVALID_ARGUMENT,
        MXS_REGANVIL_INSUFFICIENT_CAPACITY, MXS_REGANVIL_INVALID_ARGUMENT,
        MXS_REGANVIL_UNALIGNED_OFFSET, MXS_REGANVIL_DUPLICATE_OFFSET,
        MXS_REGANVIL_INVALID_MASK, MXS_REGANVIL_INVALID_MASK,
        MXS_REGANVIL_INVALID_MASK, MXS_REGANVIL_INVALID_MASK,
        MXS_REGANVIL_INVALID_RESET_VALUE
    };
    mxs_reganvil_descriptor descriptors[3];
    mxs_reganvil_context context;
    mxs_reganvil_context context_before;
    uint32_t existing_state[3];
    uint32_t existing_before[3];
    uint32_t candidate_state[5];
    uint32_t candidate_before[5];
    mxs_reganvil_trace_record trace[3];
    mxs_reganvil_trace_record trace_before[3];
    size_t test_case;

    memset(&context, 0xa6, sizeof(context));
    memset(trace, 0x93, sizeof(trace));
    REQUIRE_OK(mxs_reganvil_init(&context, valid, 3, existing_state, 3, trace, 3));
    STATUS(mxs_reganvil_write(&context, 12, 2), MXS_REGANVIL_OK);
    memcpy(&context_before, &context, sizeof(context));
    memcpy(existing_before, existing_state, sizeof(existing_state));
    memcpy(trace_before, trace, sizeof(trace));

    for (test_case = 0; test_case < sizeof(expected) / sizeof(expected[0]); ++test_case) {
        mxs_reganvil_context *context_arg = &context;
        const mxs_reganvil_descriptor *descriptor_arg = descriptors;
        uint32_t *state_arg = &candidate_state[1];
        mxs_reganvil_trace_record *trace_arg = trace;
        size_t count = 3;
        size_t state_capacity = 3;
        size_t trace_capacity = 3;

        memcpy(descriptors, valid, sizeof(descriptors));
        memset(candidate_state, 0x5a, sizeof(candidate_state));
        memcpy(candidate_before, candidate_state, sizeof(candidate_state));
        switch (test_case) {
        case 0: context_arg = NULL; break;
        case 1: descriptor_arg = NULL; break;
        case 2: state_arg = NULL; break;
        case 3: count = 0; break;
        case 4: state_capacity = 2; break;
        case 5: trace_arg = NULL; break;
        case 6: descriptors[2].offset = UINT32_C(0xfffffffe); break;
        case 7: descriptors[2].offset = descriptors[0].offset; break;
        case 8: descriptors[2].ro_mask |= UINT32_C(1); break;
        case 9: descriptors[2].w1c_mask |= UINT32_C(2); break;
        case 10: descriptors[2].w1c_mask |= UINT32_C(4); break;
        case 11: descriptors[2].read_clear_mask = UINT32_C(0x80000000); break;
        case 12: descriptors[2].reset_value = UINT32_C(0x80000000); break;
        default: CHECK(0); break;
        }
        STATUS(mxs_reganvil_init(context_arg, descriptor_arg, count, state_arg,
                                 state_capacity, trace_arg, trace_capacity),
               expected[test_case]);
        CHECK(memcmp(&context, &context_before, sizeof(context)) == 0);
        CHECK(memcmp(existing_state, existing_before, sizeof(existing_state)) == 0);
        CHECK(memcmp(candidate_state, candidate_before, sizeof(candidate_state)) == 0);
        CHECK(memcmp(trace, trace_before, sizeof(trace)) == 0);
    }
    /* The pre-existing model still works after every rejected configuration. */
    expect_peek(&context, 12, 2);
    expect_trace_info(&context, 1, 0, 3);
    STATUS(mxs_reganvil_write(&context, UINT32_C(0xfffffffc), 0), MXS_REGANVIL_OK);
    expect_peek(&context, UINT32_C(0xfffffffc), 0);
}

static void test_runtime_failures(void)
{
    const mxs_reganvil_descriptor descriptor = {0, UINT32_C(0x43), UINT32_C(0xff),
                                                0, 0, UINT32_C(1)};
    mxs_reganvil_context context;
    mxs_reganvil_context context_before;
    mxs_reganvil_trace_record trace[4];
    mxs_reganvil_trace_record trace_before[4];
    mxs_reganvil_trace_record output_record;
    mxs_reganvil_trace_record output_record_before;
    mxs_reganvil_trace_info output_info;
    mxs_reganvil_trace_info output_info_before;
    uint32_t state;
    uint32_t output = UINT32_C(0xdecafbad);

    memset(&context, 0, sizeof(context));
    memset(trace, 0xab, sizeof(trace));
    REQUIRE_OK(mxs_reganvil_init(&context, &descriptor, 1, &state, 1, trace, 4));
    STATUS(mxs_reganvil_write(&context, 0, UINT32_C(0x43)), MXS_REGANVIL_OK);
    memcpy(&context_before, &context, sizeof(context));
    memcpy(trace_before, trace, sizeof(trace));
    memset(&output_record, 0xc7, sizeof(output_record));
    memcpy(&output_record_before, &output_record, sizeof(output_record));
    memset(&output_info, 0xe3, sizeof(output_info));
    memcpy(&output_info_before, &output_info, sizeof(output_info));

    STATUS(mxs_reganvil_read(&context, 4, &output), MXS_REGANVIL_UNKNOWN_OFFSET);
    VALUE(output, UINT32_C(0xdecafbad));
    STATUS(mxs_reganvil_peek(&context, 4, &output), MXS_REGANVIL_UNKNOWN_OFFSET);
    VALUE(output, UINT32_C(0xdecafbad));
    STATUS(mxs_reganvil_write(&context, 4, 0), MXS_REGANVIL_UNKNOWN_OFFSET);
    STATUS(mxs_reganvil_hardware_update(&context, 4, 1, 0), MXS_REGANVIL_UNKNOWN_OFFSET);
    STATUS(mxs_reganvil_read(&context, 1, &output), MXS_REGANVIL_UNALIGNED_OFFSET);
    VALUE(output, UINT32_C(0xdecafbad));
    STATUS(mxs_reganvil_peek(&context, 3, &output), MXS_REGANVIL_UNALIGNED_OFFSET);
    VALUE(output, UINT32_C(0xdecafbad));
    STATUS(mxs_reganvil_write(&context, 2, 0), MXS_REGANVIL_UNALIGNED_OFFSET);
    STATUS(mxs_reganvil_hardware_update(&context, 1, 1, 0), MXS_REGANVIL_UNALIGNED_OFFSET);
    STATUS(mxs_reganvil_read(&context, 0, NULL), MXS_REGANVIL_INVALID_ARGUMENT);
    STATUS(mxs_reganvil_peek(&context, 0, NULL), MXS_REGANVIL_INVALID_ARGUMENT);
    STATUS(mxs_reganvil_trace_get_info(&context, NULL), MXS_REGANVIL_INVALID_ARGUMENT);
    STATUS(mxs_reganvil_trace_get(&context, 0, NULL), MXS_REGANVIL_INVALID_ARGUMENT);
    STATUS(mxs_reganvil_trace_get(&context, 1, &output_record), MXS_REGANVIL_OUT_OF_RANGE);
    CHECK(memcmp(&output_record, &output_record_before, sizeof(output_record)) == 0);
    STATUS(mxs_reganvil_trace_get(&context, SIZE_MAX, &output_record), MXS_REGANVIL_OUT_OF_RANGE);
    CHECK(memcmp(&output_record, &output_record_before, sizeof(output_record)) == 0);

    STATUS(mxs_reganvil_reset(NULL), MXS_REGANVIL_INVALID_ARGUMENT);
    STATUS(mxs_reganvil_read(NULL, 0, &output), MXS_REGANVIL_INVALID_ARGUMENT);
    VALUE(output, UINT32_C(0xdecafbad));
    STATUS(mxs_reganvil_write(NULL, 0, 0), MXS_REGANVIL_INVALID_ARGUMENT);
    STATUS(mxs_reganvil_peek(NULL, 0, &output), MXS_REGANVIL_INVALID_ARGUMENT);
    VALUE(output, UINT32_C(0xdecafbad));
    STATUS(mxs_reganvil_hardware_update(NULL, 0, 1, 0), MXS_REGANVIL_INVALID_ARGUMENT);
    STATUS(mxs_reganvil_trace_get_info(NULL, &output_info), MXS_REGANVIL_INVALID_ARGUMENT);
    CHECK(memcmp(&output_info, &output_info_before, sizeof(output_info)) == 0);
    STATUS(mxs_reganvil_trace_get(NULL, 0, &output_record), MXS_REGANVIL_INVALID_ARGUMENT);
    CHECK(memcmp(&output_record, &output_record_before, sizeof(output_record)) == 0);
    STATUS(mxs_reganvil_trace_clear(NULL), MXS_REGANVIL_INVALID_ARGUMENT);

    VALUE(state, UINT32_C(0x43));
    CHECK(memcmp(&context, &context_before, sizeof(context)) == 0);
    CHECK(memcmp(trace, trace_before, sizeof(trace)) == 0);
    expect_trace_info(&context, 1, 0, 4);
}

static void test_trace_semantics(void)
{
    const mxs_reganvil_descriptor descriptor = {
        0, UINT32_C(0xf3), UINT32_C(3), UINT32_C(0xc),
        UINT32_C(0xf0), UINT32_C(0x81)
    };
    mxs_reganvil_context context;
    struct {
        uint32_t before;
        mxs_reganvil_trace_record records[4];
        uint32_t after;
    } trace;
    uint32_t state;
    uint32_t value = 0;

    memset(&trace, 0xb4, sizeof(trace));
    trace.before = UINT32_C(0x10203040);
    trace.after = UINT32_C(0x50607080);
    REQUIRE_OK(mxs_reganvil_init(&context, &descriptor, 1, &state, 1,
                                trace.records, 4));
    STATUS(mxs_reganvil_write(&context, 0, UINT32_C(0x21)), MXS_REGANVIL_OK);
    expect_record(&context, 0, MXS_REGANVIL_WRITE, 0, UINT32_C(0x21),
                  UINT32_C(0xf3), UINT32_C(0xd1));
    expect_peek(&context, 0, UINT32_C(0xd1));
    STATUS(mxs_reganvil_hardware_update(&context, 0, 4, UINT32_C(0x10)), MXS_REGANVIL_OK);
    expect_peek(&context, 0, UINT32_C(0xc5));
    expect_trace_info(&context, 1, 0, 4);
    STATUS(mxs_reganvil_read(&context, 0, &value), MXS_REGANVIL_OK);
    VALUE(value, UINT32_C(0xc5));
    expect_record(&context, 1, MXS_REGANVIL_READ, 0, UINT32_C(0xc5),
                  UINT32_C(0xc5), UINT32_C(0x44));
    STATUS(mxs_reganvil_write(&context, 0, 0), MXS_REGANVIL_OK);
    expect_record(&context, 2, MXS_REGANVIL_WRITE, 0, 0,
                  UINT32_C(0x44), UINT32_C(0x44));
    STATUS(mxs_reganvil_read(&context, 0, &value), MXS_REGANVIL_OK);
    expect_record(&context, 3, MXS_REGANVIL_READ, 0, UINT32_C(0x44),
                  UINT32_C(0x44), UINT32_C(0x44));
    expect_trace_info(&context, 4, 0, 4);
    STATUS(mxs_reganvil_write(&context, 0, UINT32_MAX), MXS_REGANVIL_OK);
    expect_peek(&context, 0, 7);
    expect_trace_info(&context, 4, 1, 4);
    STATUS(mxs_reganvil_read(&context, 0, &value), MXS_REGANVIL_OK);
    VALUE(value, 7);
    expect_peek(&context, 0, 6);
    expect_trace_info(&context, 4, 2, 4);
    /* Overflow must preserve the earliest records, including their values. */
    expect_record(&context, 0, MXS_REGANVIL_WRITE, 0, UINT32_C(0x21),
                  UINT32_C(0xf3), UINT32_C(0xd1));
    expect_record(&context, 3, MXS_REGANVIL_READ, 0, UINT32_C(0x44),
                  UINT32_C(0x44), UINT32_C(0x44));
    STATUS(mxs_reganvil_trace_clear(&context), MXS_REGANVIL_OK);
    expect_peek(&context, 0, 6);
    expect_trace_info(&context, 0, 0, 4);
    STATUS(mxs_reganvil_write(&context, 0, 2), MXS_REGANVIL_OK);
    expect_record(&context, 0, MXS_REGANVIL_WRITE, 0, 2, 6, 6);
    STATUS(mxs_reganvil_reset(&context), MXS_REGANVIL_OK);
    expect_peek(&context, 0, UINT32_C(0xf3));
    expect_trace_info(&context, 0, 0, 4);
    VALUE(trace.before, UINT32_C(0x10203040));
    VALUE(trace.after, UINT32_C(0x50607080));
}

static void test_trace_capacity_one_and_saturation(void)
{
    const mxs_reganvil_descriptor descriptor = {0, 0, UINT32_MAX, 0, 0, 0};
    mxs_reganvil_context context;
    struct {
        uint32_t before;
        mxs_reganvil_trace_record records[2];
        uint32_t after;
    } trace;
    mxs_reganvil_trace_record unused_before;
    uint32_t state;
    uint32_t value = 0;

    memset(&trace, 0x4d, sizeof(trace));
    trace.before = UINT32_C(0xaabbccdd);
    trace.after = UINT32_C(0xddccbbaa);
    memcpy(&unused_before, &trace.records[1], sizeof(unused_before));
    REQUIRE_OK(mxs_reganvil_init(&context, &descriptor, 1, &state, 1,
                                trace.records, 1));
    STATUS(mxs_reganvil_write(&context, 0, UINT32_C(0x80000001)), MXS_REGANVIL_OK);
    STATUS(mxs_reganvil_write(&context, 0, UINT32_MAX), MXS_REGANVIL_OK);
    expect_peek(&context, 0, UINT32_MAX);
    expect_trace_info(&context, 1, 1, 1);
    expect_record(&context, 0, MXS_REGANVIL_WRITE, 0, UINT32_C(0x80000001),
                  0, UINT32_C(0x80000001));
    /* White-box setup makes the size_t saturation boundary test bounded. */
    context.trace_dropped = SIZE_MAX - 1;
    STATUS(mxs_reganvil_read(&context, 0, &value), MXS_REGANVIL_OK);
    VALUE(value, UINT32_MAX);
    expect_trace_info(&context, 1, SIZE_MAX, 1);
    STATUS(mxs_reganvil_write(&context, 0, 9), MXS_REGANVIL_OK);
    expect_peek(&context, 0, 9);
    expect_trace_info(&context, 1, SIZE_MAX, 1);
    CHECK(memcmp(&trace.records[1], &unused_before, sizeof(unused_before)) == 0);
    VALUE(trace.before, UINT32_C(0xaabbccdd));
    VALUE(trace.after, UINT32_C(0xddccbbaa));
    STATUS(mxs_reganvil_reset(&context), MXS_REGANVIL_OK);
    expect_trace_info(&context, 0, 0, 1);
    expect_peek(&context, 0, 0);
    STATUS(mxs_reganvil_read(&context, 0, &value), MXS_REGANVIL_OK);
    expect_record(&context, 0, MXS_REGANVIL_READ, 0, 0, 0, 0);
}

static void test_disabled_trace(void)
{
    const mxs_reganvil_descriptor descriptor = {0, 3, UINT32_C(3), 0, 0, 1};
    mxs_reganvil_context context;
    mxs_reganvil_trace_record trace;
    mxs_reganvil_trace_record trace_before;
    mxs_reganvil_trace_record output;
    mxs_reganvil_trace_record output_before;
    uint32_t state;
    uint32_t value = 0;
    size_t mode;
    size_t i;

    memset(&trace, 0x76, sizeof(trace));
    memcpy(&trace_before, &trace, sizeof(trace));
    memset(&output, 0x87, sizeof(output));
    memcpy(&output_before, &output, sizeof(output));
    for (mode = 0; mode < 2; ++mode) {
        REQUIRE_OK(mxs_reganvil_init(&context, &descriptor, 1, &state, 1,
                                    mode == 0 ? NULL : &trace, 0));
        for (i = 0; i < 16; ++i) {
            STATUS(mxs_reganvil_write(&context, 0, 3), MXS_REGANVIL_OK);
            STATUS(mxs_reganvil_read(&context, 0, &value), MXS_REGANVIL_OK);
            VALUE(value, 3);
            expect_peek(&context, 0, 2);
        }
        expect_trace_info(&context, 0, 0, 0);
        STATUS(mxs_reganvil_trace_get(&context, 0, &output), MXS_REGANVIL_OUT_OF_RANGE);
        CHECK(memcmp(&output, &output_before, sizeof(output)) == 0);
        STATUS(mxs_reganvil_trace_clear(&context), MXS_REGANVIL_OK);
        expect_peek(&context, 0, 2);
        expect_trace_info(&context, 0, 0, 0);
        STATUS(mxs_reganvil_reset(&context), MXS_REGANVIL_OK);
        expect_peek(&context, 0, 3);
        expect_trace_info(&context, 0, 0, 0);
        CHECK(memcmp(&trace, &trace_before, sizeof(trace)) == 0);
    }
}

static void test_independent_models(void)
{
    const mxs_reganvil_descriptor descriptor = {4, 3, UINT32_C(0xff), 0, 0, 1};
    mxs_reganvil_context first;
    mxs_reganvil_context second;
    uint32_t first_state;
    uint32_t second_state;
    mxs_reganvil_trace_record first_trace[2];
    mxs_reganvil_trace_record second_trace[3];
    uint32_t value = 0;

    REQUIRE_OK(mxs_reganvil_init(&first, &descriptor, 1, &first_state, 1, first_trace, 2));
    REQUIRE_OK(mxs_reganvil_init(&second, &descriptor, 1, &second_state, 1, second_trace, 3));
    STATUS(mxs_reganvil_write(&first, 4, UINT32_C(0x85)), MXS_REGANVIL_OK);
    expect_peek(&second, 4, 3);
    expect_trace_info(&second, 0, 0, 3);
    STATUS(mxs_reganvil_read(&second, 4, &value), MXS_REGANVIL_OK);
    VALUE(value, 3);
    expect_peek(&first, 4, UINT32_C(0x85));
    expect_peek(&second, 4, 2);
    expect_trace_info(&first, 1, 0, 2);
    expect_trace_info(&second, 1, 0, 3);
    STATUS(mxs_reganvil_reset(&first), MXS_REGANVIL_OK);
    expect_peek(&second, 4, 2);
    expect_trace_info(&second, 1, 0, 3);
    STATUS(mxs_reganvil_hardware_update(&second, 4, UINT32_C(0x40), 0), MXS_REGANVIL_OK);
    expect_peek(&second, 4, UINT32_C(0x42));
    expect_peek(&first, 4, 3);
    STATUS(mxs_reganvil_trace_clear(&second), MXS_REGANVIL_OK);
    expect_peek(&first, 4, 3);
}

static void test_status_strings(void)
{
    const mxs_reganvil_status statuses[] = {
        MXS_REGANVIL_OK, MXS_REGANVIL_INVALID_ARGUMENT,
        MXS_REGANVIL_INSUFFICIENT_CAPACITY, MXS_REGANVIL_UNALIGNED_OFFSET,
        MXS_REGANVIL_DUPLICATE_OFFSET, MXS_REGANVIL_INVALID_MASK,
        MXS_REGANVIL_INVALID_RESET_VALUE, MXS_REGANVIL_UNKNOWN_OFFSET,
        MXS_REGANVIL_OUT_OF_RANGE, (mxs_reganvil_status)12345
    };
    size_t i;

    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); ++i) {
        const char *text = mxs_reganvil_status_string(statuses[i]);
        CHECK(text != NULL);
        if (text != NULL) {
            CHECK(text[0] != '\0');
            CHECK(strcmp(text, mxs_reganvil_status_string(statuses[i])) == 0);
        }
    }
}

static uint32_t next_random(uint32_t *state)
{
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

/* The oracle decides each bit independently, without the production formulas. */
static uint32_t reference_write(const mxs_reganvil_descriptor *descriptor,
                                 uint32_t old, uint32_t written)
{
    uint32_t result = 0;
    unsigned int bit;
    for (bit = 0; bit < 32; ++bit) {
        uint32_t mask = UINT32_C(1) << bit;
        int bit_value = (old & mask) != 0;
        if ((descriptor->rw_mask & mask) != 0) {
            bit_value = (written & mask) != 0;
        } else if ((descriptor->w1c_mask & mask) != 0 && (written & mask) != 0) {
            bit_value = 0;
        }
        if (bit_value) {
            result |= mask;
        }
    }
    return result;
}

static uint32_t reference_read(const mxs_reganvil_descriptor *descriptor, uint32_t old)
{
    uint32_t result = 0;
    unsigned int bit;
    for (bit = 0; bit < 32; ++bit) {
        uint32_t mask = UINT32_C(1) << bit;
        if ((old & mask) != 0 && (descriptor->read_clear_mask & mask) == 0) {
            result |= mask;
        }
    }
    return result;
}

static uint32_t reference_hardware(uint32_t old, uint32_t set_bits, uint32_t clear_bits)
{
    uint32_t result = 0;
    unsigned int bit;
    for (bit = 0; bit < 32; ++bit) {
        uint32_t mask = UINT32_C(1) << bit;
        int bit_value = (old & mask) != 0;
        if ((set_bits & mask) != 0) {
            bit_value = 1;
        } else if ((clear_bits & mask) != 0) {
            bit_value = 0;
        }
        if (bit_value) {
            result |= mask;
        }
    }
    return result;
}

static void reference_append(mxs_reganvil_trace_record *records, size_t capacity,
                              size_t *count, size_t *dropped,
                              mxs_reganvil_operation operation, uint32_t offset,
                              uint32_t value, uint32_t before, uint32_t after)
{
    if (capacity == 0) {
        return;
    }
    if (*count < capacity) {
        mxs_reganvil_trace_record *record = &records[*count];
        record->operation = operation;
        record->offset = offset;
        record->value = value;
        record->before = before;
        record->after = after;
        ++*count;
    } else {
        ++*dropped;
    }
}

static void run_randomized(uint32_t seed, size_t capacity)
{
    const uint32_t offsets[] = {12, 0, 64, 4, UINT32_C(0xfffffffc)};
    mxs_reganvil_descriptor descriptors[5];
    mxs_reganvil_context context;
    struct {
        uint32_t before;
        uint32_t values[5];
        uint32_t after;
    } state;
    struct {
        uint32_t before;
        mxs_reganvil_trace_record records[4];
        uint32_t after;
    } trace;
    mxs_reganvil_trace_record expected_records[4];
    uint32_t expected_values[5];
    uint32_t generator = seed;
    size_t recorded = 0;
    size_t dropped = 0;
    size_t i;

    random_seed = seed;
    random_operation = 0;
    randomized_check = 1;
    memset(descriptors, 0, sizeof(descriptors));
    memset(&trace, 0, sizeof(trace));
    state.before = UINT32_C(0x1a2b3c4d);
    state.after = UINT32_C(0x5e6f7081);
    trace.before = UINT32_C(0x19283746);
    trace.after = UINT32_C(0x5a6b7c8d);
    for (i = 0; i < 5; ++i) {
        unsigned int bit;
        descriptors[i].offset = offsets[i];
        for (bit = 0; bit < 32; ++bit) {
            uint32_t mask = UINT32_C(1) << bit;
            uint32_t permission = next_random(&generator) % UINT32_C(4);
            if (permission == 1) {
                descriptors[i].rw_mask |= mask;
            } else if (permission == 2) {
                descriptors[i].ro_mask |= mask;
            } else if (permission == 3) {
                descriptors[i].w1c_mask |= mask;
            }
            if (permission != 0) {
                if ((next_random(&generator) & UINT32_C(1)) != 0) {
                    descriptors[i].reset_value |= mask;
                }
                if ((next_random(&generator) & UINT32_C(1)) != 0) {
                    descriptors[i].read_clear_mask |= mask;
                }
            }
        }
        expected_values[i] = descriptors[i].reset_value;
    }
    if (!check_status(mxs_reganvil_init(&context, descriptors, 5, state.values, 5,
                                        capacity == 0 ? NULL : trace.records, capacity),
                      MXS_REGANVIL_OK, __LINE__)) {
        randomized_check = 0;
        return;
    }

    for (random_operation = 0; random_operation < 2048; ++random_operation) {
        size_t index = (size_t)(next_random(&generator) % UINT32_C(5));
        uint32_t operation = next_random(&generator) % UINT32_C(10);
        uint32_t value = next_random(&generator);
        uint32_t output = UINT32_C(0xcafef00d);
        uint32_t before = expected_values[index];
        const mxs_reganvil_descriptor *descriptor = &descriptors[index];

        switch (operation) {
        case 0:
            expected_values[index] = reference_write(descriptor, before, value);
            STATUS(mxs_reganvil_write(&context, descriptor->offset, value), MXS_REGANVIL_OK);
            reference_append(expected_records, capacity, &recorded, &dropped,
                             MXS_REGANVIL_WRITE, descriptor->offset, value,
                             before, expected_values[index]);
            break;
        case 1:
            expected_values[index] = reference_read(descriptor, before);
            STATUS(mxs_reganvil_read(&context, descriptor->offset, &output), MXS_REGANVIL_OK);
            VALUE(output, before);
            reference_append(expected_records, capacity, &recorded, &dropped,
                             MXS_REGANVIL_READ, descriptor->offset, before,
                             before, expected_values[index]);
            break;
        case 2:
            STATUS(mxs_reganvil_peek(&context, descriptor->offset, &output), MXS_REGANVIL_OK);
            VALUE(output, before);
            break;
        case 3: {
            uint32_t set_bits = 0;
            uint32_t clear_bits = 0;
            unsigned int bit;
            for (bit = 0; bit < 32; ++bit) {
                uint32_t mask = UINT32_C(1) << bit;
                if ((descriptor->rw_mask & mask) != 0 ||
                    (descriptor->ro_mask & mask) != 0 ||
                    (descriptor->w1c_mask & mask) != 0) {
                    uint32_t action = next_random(&generator) % UINT32_C(3);
                    if (action == 1) {
                        set_bits |= mask;
                    } else if (action == 2) {
                        clear_bits |= mask;
                    }
                }
            }
            expected_values[index] = reference_hardware(before, set_bits, clear_bits);
            STATUS(mxs_reganvil_hardware_update(&context, descriptor->offset,
                                                set_bits, clear_bits), MXS_REGANVIL_OK);
            break;
        }
        case 4: {
            uint32_t overlap = UINT32_C(1) << (value % UINT32_C(32));
            STATUS(mxs_reganvil_hardware_update(&context, descriptor->offset,
                                                overlap, overlap), MXS_REGANVIL_INVALID_MASK);
            break;
        }
        case 5:
            if ((value & UINT32_C(1)) != 0) {
                STATUS(mxs_reganvil_read(&context, UINT32_C(0xffffffe0), &output),
                       MXS_REGANVIL_UNKNOWN_OFFSET);
                VALUE(output, UINT32_C(0xcafef00d));
            } else {
                STATUS(mxs_reganvil_write(&context, UINT32_C(0xffffffe0), value),
                       MXS_REGANVIL_UNKNOWN_OFFSET);
            }
            break;
        case 6:
            if ((value & UINT32_C(1)) != 0) {
                STATUS(mxs_reganvil_read(&context, descriptor->offset + 1, &output),
                       MXS_REGANVIL_UNALIGNED_OFFSET);
                VALUE(output, UINT32_C(0xcafef00d));
            } else {
                STATUS(mxs_reganvil_write(&context, descriptor->offset + 2, value),
                       MXS_REGANVIL_UNALIGNED_OFFSET);
            }
            break;
        case 7:
            STATUS(mxs_reganvil_trace_clear(&context), MXS_REGANVIL_OK);
            recorded = 0;
            dropped = 0;
            break;
        case 8:
            STATUS(mxs_reganvil_reset(&context), MXS_REGANVIL_OK);
            for (i = 0; i < 5; ++i) {
                expected_values[i] = descriptors[i].reset_value;
            }
            recorded = 0;
            dropped = 0;
            break;
        case 9: {
            uint32_t reserved = 0;
            unsigned int bit;
            for (bit = 0; bit < 32; ++bit) {
                uint32_t mask = UINT32_C(1) << bit;
                if ((descriptor->rw_mask & mask) == 0 &&
                    (descriptor->ro_mask & mask) == 0 &&
                    (descriptor->w1c_mask & mask) == 0) {
                    reserved = mask;
                    break;
                }
            }
            if (reserved != 0) {
                STATUS(mxs_reganvil_hardware_update(&context, descriptor->offset,
                                                    reserved, 0), MXS_REGANVIL_INVALID_MASK);
            } else {
                STATUS(mxs_reganvil_hardware_update(&context, descriptor->offset, 1, 1),
                       MXS_REGANVIL_INVALID_MASK);
            }
            break;
        }
        default: CHECK(0); break;
        }

        for (i = 0; i < 5; ++i) {
            VALUE(state.values[i], expected_values[i]);
            expect_peek(&context, descriptors[i].offset, expected_values[i]);
        }
        expect_trace_info(&context, recorded, dropped, capacity);
        for (i = 0; i < recorded; ++i) {
            const mxs_reganvil_trace_record *record = &expected_records[i];
            expect_record(&context, i, record->operation, record->offset,
                          record->value, record->before, record->after);
        }
        VALUE(state.before, UINT32_C(0x1a2b3c4d));
        VALUE(state.after, UINT32_C(0x5e6f7081));
        VALUE(trace.before, UINT32_C(0x19283746));
        VALUE(trace.after, UINT32_C(0x5a6b7c8d));
        if (failures != 0) {
            break;
        }
    }
    randomized_check = 0;
}

int main(void)
{
    test_initialization_and_reset();
    test_software_semantics();
    test_hardware_updates();
    test_initialization_failures();
    test_runtime_failures();
    test_trace_semantics();
    test_trace_capacity_one_and_saturation();
    test_disabled_trace();
    test_independent_models();
    test_status_strings();
    run_randomized(UINT32_C(0x5eed1234), 4);
    run_randomized(UINT32_C(0x91c4a7d3), 1);
    run_randomized(UINT32_C(0x743eb821), 0);
    if (failures != 0) {
        fprintf(stderr, "RegAnvil: %zu of %zu checks failed.\n", failures, checks_run);
        return EXIT_FAILURE;
    }
    printf("RegAnvil: %zu checks passed (6144 deterministic random operations).\n", checks_run);
    return EXIT_SUCCESS;
}
