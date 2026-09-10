/* Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT */
#include "mxs_reganvil.h"
#include "rp2040_irq_driver.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned long checks;

/* Always active, including Release builds compiled with NDEBUG. */
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
            __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

/* Literal offsets and masks make this description independent of the driver's
 * constants. The hardware fixture supplies levels/edges explicitly; it does
 * not claim to derive them from GPIO electrical signals or other registers.
 */
static const mxs_reganvil_descriptor registers[] = {
    { UINT32_C(0x0f0), 0, 0, UINT32_C(0x33333333), UINT32_C(0xcccccccc), 0 },
    { UINT32_C(0x100), 0, UINT32_MAX, 0, 0, 0 }
};

typedef struct fixture {
    mxs_reganvil_context model;
    uint32_t state[2];
    mxs_reganvil_trace_record trace[32];
    mxs_reganvil_rp2040_irq_driver driver;
    unsigned int reads;
    unsigned int writes;
    unsigned int fail_read;
    unsigned int fail_write;
    int failure;
} fixture;

static int adapter_read(void *context, uint32_t offset, uint32_t *value)
{
    fixture *f = (fixture *)context;
    ++f->reads;
    if (f->fail_read == f->reads) {
        /* A failing callback need not preserve its own output; the driver must
         * keep the caller's output safe by using a local intermediate value.
         */
        *value = UINT32_C(0xdeadbeef);
        return f->failure;
    }
    return (int)mxs_reganvil_read(&f->model, offset, value);
}

static int adapter_write(void *context, uint32_t offset, uint32_t value)
{
    fixture *f = (fixture *)context;
    ++f->writes;
    if (f->fail_write == f->writes) {
        return f->failure;
    }
    return (int)mxs_reganvil_write(&f->model, offset, value);
}

static void init(fixture *f)
{
    CHECK(mxs_reganvil_init(&f->model, registers, 2, f->state, 2,
        f->trace, 32) == MXS_REGANVIL_OK);
    f->driver.context = f;
    f->driver.read = adapter_read;
    f->driver.write = adapter_write;
    f->reads = 0;
    f->writes = 0;
    f->fail_read = 0;
    f->fail_write = 0;
    f->failure = -73;
}

static uint32_t peek(fixture *f, uint32_t offset)
{
    uint32_t value = 0;
    CHECK(mxs_reganvil_peek(&f->model, offset, &value) == MXS_REGANVIL_OK);
    return value;
}

static void set_raw(fixture *f, uint32_t value)
{
    CHECK(mxs_reganvil_hardware_update(&f->model, UINT32_C(0x0f0),
        value, ~value) == MXS_REGANVIL_OK);
}

static void expect_trace(fixture *f, size_t index,
    mxs_reganvil_operation operation, uint32_t offset,
    uint32_t value, uint32_t before, uint32_t after)
{
    mxs_reganvil_trace_record record;
    CHECK(mxs_reganvil_trace_get(&f->model, index, &record) == MXS_REGANVIL_OK);
    CHECK(record.operation == operation);
    CHECK(record.offset == offset);
    CHECK(record.value == value);
    CHECK(record.before == before);
    CHECK(record.after == after);
}

static void test_interrupt_lifecycle(void)
{
    fixture f;
    uint32_t acknowledged = UINT32_MAX;
    uint32_t events = 0;
    mxs_reganvil_trace_info info;

    init(&f);
    CHECK(peek(&f, UINT32_C(0x0f0)) == 0);
    CHECK(peek(&f, UINT32_C(0x100)) == 0);

    /* Enable GPIO0 rising and both GPIO7 edges. GPIO1 rising stays disabled. */
    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 0, 8, 1) == 0);
    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 7, 12, 1) == 0);
    CHECK(peek(&f, UINT32_C(0x100)) == UINT32_C(0xc0000008));
    /* GPIO0 is high, GPIO7 low, and four edge latches are pending. */
    set_raw(&f, UINT32_C(0xd000008a));
    CHECK(mxs_reganvil_rp2040_irq_raw(&f.driver, 0, &events) == 0);
    CHECK(events == 10);
    CHECK(mxs_reganvil_rp2040_irq_raw(&f.driver, 7, &events) == 0);
    CHECK(events == 13);
    CHECK(peek(&f, UINT32_C(0x0f0)) == UINT32_C(0xd000008a));

    CHECK(mxs_reganvil_trace_clear(&f.model) == MXS_REGANVIL_OK);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&f.driver,
        UINT32_C(0x40000088), &acknowledged) == 0);
    CHECK(acknowledged == UINT32_C(0x40000008));
    /* Unrequested bit31, disabled GPIO1 rising, and both RO levels survive. */
    CHECK(peek(&f, UINT32_C(0x0f0)) == UINT32_C(0x90000082));
    expect_trace(&f, 0, MXS_REGANVIL_READ, UINT32_C(0x100),
        UINT32_C(0xc0000008), UINT32_C(0xc0000008), UINT32_C(0xc0000008));
    expect_trace(&f, 1, MXS_REGANVIL_READ, UINT32_C(0x0f0),
        UINT32_C(0xd000008a), UINT32_C(0xd000008a), UINT32_C(0xd000008a));
    expect_trace(&f, 2, MXS_REGANVIL_WRITE, UINT32_C(0x0f0),
        UINT32_C(0x40000008), UINT32_C(0xd000008a), UINT32_C(0x90000082));

    /* Repeating the take returns no events and emits no write. */
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&f.driver,
        UINT32_C(0x40000088), &acknowledged) == 0);
    CHECK(acknowledged == 0);
    CHECK(mxs_reganvil_trace_get_info(&f.model, &info) == MXS_REGANVIL_OK);
    CHECK(info.recorded_count == 5);

    /* A fresh event arrives after the prior acknowledgement completes. */
    CHECK(mxs_reganvil_hardware_update(&f.model, UINT32_C(0x0f0),
        UINT32_C(8), 0) == MXS_REGANVIL_OK);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&f.driver,
        UINT32_C(8), &acknowledged) == 0);
    CHECK(acknowledged == 8);
    CHECK(peek(&f, UINT32_C(0x0f0)) == UINT32_C(0x90000082));

    CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver, 7, 8) == 0);
    CHECK(peek(&f, UINT32_C(0x0f0)) == UINT32_C(0x10000082));
    CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver, 7, 8) == 0);
    CHECK(peek(&f, UINT32_C(0x0f0)) == UINT32_C(0x10000082));

    /* Software writes cannot dismiss a level condition. */
    CHECK(mxs_reganvil_write(&f.model, UINT32_C(0x0f0),
        UINT32_C(0x33333333)) == MXS_REGANVIL_OK);
    CHECK(peek(&f, UINT32_C(0x0f0)) == UINT32_C(0x10000082));
    /* Hardware explicitly changes GPIO0's modeled level from high to low. */
    CHECK(mxs_reganvil_hardware_update(&f.model, UINT32_C(0x0f0),
        UINT32_C(1), UINT32_C(2)) == MXS_REGANVIL_OK);
    CHECK(mxs_reganvil_rp2040_irq_raw(&f.driver, 0, &events) == 0);
    CHECK(events == 1);

    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 7, 12, 0) == 0);
    CHECK(peek(&f, UINT32_C(0x100)) == 8);
    CHECK(mxs_reganvil_reset(&f.model) == MXS_REGANVIL_OK);
    CHECK(peek(&f, UINT32_C(0x0f0)) == 0);
    CHECK(peek(&f, UINT32_C(0x100)) == 0);
    CHECK(mxs_reganvil_trace_get_info(&f.model, &info) == MXS_REGANVIL_OK);
    CHECK(info.recorded_count == 0);
    CHECK(info.dropped_count == 0);
}

static void test_pin_masks(void)
{
    unsigned int pin;
    uint32_t events;
    int enabled;

    for (pin = 0; pin < 8U; ++pin) {
        for (events = 0; events < 16; ++events) {
            for (enabled = 0; enabled <= 1; ++enabled) {
                fixture f;
                uint32_t original = UINT32_C(0xa55aa55a);
                uint32_t mask = events << (4U * pin);
                uint32_t expected = enabled != 0 ?
                    original | mask : original & ~mask;
                init(&f);
                CHECK(mxs_reganvil_write(&f.model, UINT32_C(0x100),
                    original) == MXS_REGANVIL_OK);
                CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver,
                    pin, events, enabled) == 0);
                CHECK(peek(&f, UINT32_C(0x100)) == expected);
                CHECK(f.reads == (events != 0 ? 1U : 0U));
                CHECK(f.writes == (events != 0 ? 1U : 0U));
            }
        }
        /* Each possible edge subset for each pin, including GPIO7 bit31. */
        for (events = 0; events <= 12; events += 4) {
            fixture f;
            uint32_t raw_events = 0;
            uint32_t original = UINT32_C(0xeeeeeeee);
            uint32_t cleared = events << (4U * pin);
            init(&f);
            set_raw(&f, original);
            CHECK(mxs_reganvil_rp2040_irq_raw(&f.driver, pin,
                &raw_events) == 0);
            CHECK(raw_events == 14);
            CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver, pin, events) == 0);
            CHECK(peek(&f, UINT32_C(0x0f0)) == (original & ~cleared));
            CHECK(f.reads == 1);
            CHECK(f.writes == (events != 0 ? 1U : 0U));
        }
    }
}

static void test_invalid_arguments(void)
{
    fixture f;
    mxs_reganvil_rp2040_irq_driver bad;
    uint32_t output = UINT32_C(0x87654321);
    const unsigned int invalid_pins[] = { 8U, 31U, 32U, UINT_MAX };
    size_t i;

    init(&f);
    CHECK(mxs_reganvil_rp2040_irq_enable(NULL, 0, 8, 1) == -1);
    CHECK(mxs_reganvil_rp2040_irq_raw(NULL, 0, &output) == -1);
    CHECK(mxs_reganvil_rp2040_irq_ack(NULL, 0, 8) == -1);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(NULL, 8, &output) == -1);
    CHECK(mxs_reganvil_rp2040_irq_raw(&f.driver, 0, NULL) == -1);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&f.driver, 8, NULL) == -1);
    for (i = 0; i < sizeof(invalid_pins) / sizeof(invalid_pins[0]); ++i) {
        CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver,
            invalid_pins[i], 8, 1) == -1);
        CHECK(mxs_reganvil_rp2040_irq_raw(&f.driver,
            invalid_pins[i], &output) == -1);
        CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver,
            invalid_pins[i], 8) == -1);
    }
    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 0, 16, 1) == -1);
    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 0, 8, -1) == -1);
    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 0, 8, 2) == -1);
    CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver, 0, 1) == -1);
    CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver, 0, 2) == -1);
    CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver, 0, UINT32_MAX) == -1);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&f.driver,
        UINT32_C(0x20000000), &output) == -1);

    bad = f.driver;
    bad.read = NULL;
    CHECK(mxs_reganvil_rp2040_irq_enable(&bad, 0, 8, 1) == -1);
    CHECK(mxs_reganvil_rp2040_irq_raw(&bad, 0, &output) == -1);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&bad, 8, &output) == -1);
    bad = f.driver;
    bad.write = NULL;
    CHECK(mxs_reganvil_rp2040_irq_enable(&bad, 0, 8, 1) == -1);
    CHECK(mxs_reganvil_rp2040_irq_ack(&bad, 0, 8) == -1);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&bad, 8, &output) == -1);
    CHECK(output == UINT32_C(0x87654321));
    CHECK(f.reads == 0 && f.writes == 0);
    CHECK(peek(&f, UINT32_C(0x0f0)) == 0);
    CHECK(peek(&f, UINT32_C(0x100)) == 0);

    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 7, 0, 1) == 0);
    CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver, 7, 0) == 0);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&f.driver, 0, &output) == 0);
    CHECK(output == 0);
    CHECK(f.reads == 0 && f.writes == 0);
}

static void test_callback_failures(void)
{
    fixture f;
    uint32_t output;
    unsigned int stage;

    /* Every access in take() can fail; report the exact error, preserve the
     * caller output, stop further callbacks, and keep this fixture's state.
     */
    for (stage = 1; stage <= 3U; ++stage) {
        init(&f);
        set_raw(&f, UINT32_C(0x80000008));
        CHECK(mxs_reganvil_write(&f.model, UINT32_C(0x100),
            UINT32_C(0x80000008)) == MXS_REGANVIL_OK);
        output = UINT32_C(0x12345678);
        if (stage <= 2U) {
            f.fail_read = stage;
        } else {
            f.fail_write = 1;
        }
        f.failure = stage == 2U ? 73 : -73;
        CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&f.driver,
            UINT32_C(0x80000008), &output) == f.failure);
        CHECK(output == UINT32_C(0x12345678));
        CHECK(f.reads == (stage < 3U ? stage : 2U));
        CHECK(f.writes == (stage == 3U ? 1U : 0U));
        CHECK(peek(&f, UINT32_C(0x0f0)) == UINT32_C(0x80000008));
        CHECK(peek(&f, UINT32_C(0x100)) == UINT32_C(0x80000008));
    }

    init(&f);
    f.fail_read = 1;
    output = UINT32_C(0x12345678);
    CHECK(mxs_reganvil_rp2040_irq_raw(&f.driver, 0, &output) == -73);
    CHECK(output == UINT32_C(0x12345678));
    CHECK(f.reads == 1 && f.writes == 0);

    init(&f);
    f.fail_read = 1;
    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 7, 8, 1) == -73);
    CHECK(f.reads == 1 && f.writes == 0);
    CHECK(peek(&f, UINT32_C(0x100)) == 0);

    init(&f);
    f.fail_write = 1;
    CHECK(mxs_reganvil_rp2040_irq_enable(&f.driver, 7, 8, 1) == -73);
    CHECK(f.reads == 1 && f.writes == 1);
    CHECK(peek(&f, UINT32_C(0x100)) == 0);

    init(&f);
    set_raw(&f, UINT32_C(0x80000000));
    f.fail_write = 1;
    CHECK(mxs_reganvil_rp2040_irq_ack(&f.driver, 7, 8) == -73);
    CHECK(f.reads == 0 && f.writes == 1);
    CHECK(peek(&f, UINT32_C(0x0f0)) == UINT32_C(0x80000000));
}

static void test_independent_instances_and_trace_overflow(void)
{
    fixture a;
    fixture b;
    uint32_t acknowledged = UINT32_MAX;
    mxs_reganvil_trace_info info;
    unsigned int i;

    init(&a);
    init(&b);
    CHECK(mxs_reganvil_rp2040_irq_enable(&a.driver, 0, 8, 1) == 0);
    CHECK(mxs_reganvil_rp2040_irq_enable(&b.driver, 7, 8, 1) == 0);
    set_raw(&a, UINT32_C(0x80000008));
    set_raw(&b, UINT32_C(0x80000008));
    for (i = 0; i < 40U; ++i) {
        CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&a.driver,
            UINT32_C(0x80000008), &acknowledged) == 0);
        CHECK(acknowledged == (i == 0 ? UINT32_C(8) : UINT32_C(0)));
    }
    CHECK(peek(&a, UINT32_C(0x0f0)) == UINT32_C(0x80000000));
    CHECK(peek(&b, UINT32_C(0x0f0)) == UINT32_C(0x80000008));
    CHECK(mxs_reganvil_trace_get_info(&a.model, &info) == MXS_REGANVIL_OK);
    CHECK(info.recorded_count == 32);
    CHECK(info.dropped_count == 51);
    CHECK(mxs_reganvil_rp2040_irq_take_enabled_edges(&b.driver,
        UINT32_C(0x80000008), &acknowledged) == 0);
    CHECK(acknowledged == UINT32_C(0x80000000));
    CHECK(peek(&b, UINT32_C(0x0f0)) == 8);
    CHECK(mxs_reganvil_reset(&a.model) == MXS_REGANVIL_OK);
    CHECK(peek(&b, UINT32_C(0x0f0)) == 8);
    CHECK(peek(&b, UINT32_C(0x100)) == UINT32_C(0x80000000));
}

int main(void)
{
    test_interrupt_lifecycle();
    test_pin_masks();
    test_invalid_arguments();
    test_callback_failures();
    test_independent_instances_and_trace_overflow();
    printf("RP2040 GPIO0-7 register-level driver integration: %lu checks passed\n",
        checks);
    return EXIT_SUCCESS;
}
