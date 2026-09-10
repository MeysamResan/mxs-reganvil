# Practical guides: testing embedded C register access

RegAnvil models 32-bit memory-mapped I/O (MMIO) register behavior inside native C tests. These guides show how to test an embedded driver without hardware, verify write-one-to-clear interrupt acknowledgement, and check read-clear side effects.

Each example is a complete C99 program for RegAnvil 0.1.0. Its checks remain active with `NDEBUG`. Save the program under the suggested filename in the repository root, compile it with the command shown, and run it. Success prints `PASS` and exits zero. On Windows, run the corresponding `.exe` file.

[Full user guide](../README.md) · [Public API contract](../include/mxs_reganvil.h)

## Unit test MMIO access in an embedded C driver without hardware

Give the driver a replaceable register accessor. In production, it talks to the device. In a native test, it calls RegAnvil. This example checks a data-ready status flag. The test raises the flag through a simulated hardware update even though the register is read-only to software.

Save as `guide_mmio.c`:

```c
#include "mxs_reganvil.h"
#include <stdio.h>
#include <stdlib.h>

typedef int (*read_register_fn)(void *, uint32_t, uint32_t *);

/* Driver code: callbacks return zero on success. */
static int data_ready(void *context, read_register_fn read_register,
                      uint32_t *ready)
{
    uint32_t status;
    int result = read_register(context, UINT32_C(0x04), &status);
    if (result == 0) {
        *ready = status & UINT32_C(1);
    }
    return result;
}

/* Test adapter: production supplies a device-specific accessor. */
static int model_read(void *context, uint32_t offset, uint32_t *value)
{
    return (int)mxs_reganvil_read(
        (mxs_reganvil_context *)context, offset, value);
}

int main(void)
{
    const mxs_reganvil_descriptor registers[] = {
        { .offset = UINT32_C(0x04), .ro_mask = UINT32_C(1) }
    };
    mxs_reganvil_context model;
    uint32_t state[1];
    uint32_t ready = UINT32_C(0);

    if (mxs_reganvil_init(&model, registers, 1, state, 1, NULL, 0)
            != MXS_REGANVIL_OK ||
        mxs_reganvil_hardware_update(
            &model, UINT32_C(0x04), UINT32_C(1), UINT32_C(0))
            != MXS_REGANVIL_OK ||
        data_ready(&model, model_read, &ready) != 0 ||
        ready != UINT32_C(1)) {
        return EXIT_FAILURE;
    }
    puts("PASS: the driver observed the hardware data-ready flag.");
    return EXIT_SUCCESS;
}
```

```sh
cc -std=c99 -Wall -Wextra -Wpedantic -Iinclude src/mxs_reganvil.c guide_mmio.c -o guide_mmio
./guide_mmio
```

The descriptor's omitted fields initialize to zero. The model starts with the flag clear. `hardware_update` sets it independently of the bit's read-only software permission. The `data_ready` function depends only on the callback signature and can live in the real driver.

Model only the registers the test needs, use offsets relative to the device base address, and propagate or check accessor errors. The model does not intercept arbitrary volatile pointer accesses. See the [callback driver](../examples/interrupt_driver.c) and [RP2040 fixture](../tests/driver/README.md) for larger integrations.

## Test write-one-to-clear interrupt acknowledgement

A write-one-to-clear (W1C) bit clears when software writes one. Writing zero leaves it unchanged. If flags A and B are pending (`0x03`), acknowledge only A by writing `0x01`. A read/modify/write that submits `0x03` acknowledges B too.

| Starting value | Submitted write | Result | Meaning |
| --- | --- | --- | --- |
| `0x03` | `0x01` | `0x02` | Only A was acknowledged |
| `0x03` | `0x03` | `0x00` | Both flags were acknowledged |

Save as `guide_w1c.c`:

```c
#include "mxs_reganvil.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const mxs_reganvil_descriptor registers[] = {
        {
            .offset = UINT32_C(0x10),
            .reset_value = UINT32_C(0x03),
            .w1c_mask = UINT32_C(0x03)
        }
    };
    mxs_reganvil_context model;
    uint32_t state[1];
    uint32_t pending = UINT32_C(0);

    if (mxs_reganvil_init(&model, registers, 1, state, 1, NULL, 0)
            != MXS_REGANVIL_OK ||
        mxs_reganvil_write(&model, UINT32_C(0x10), UINT32_C(0x01))
            != MXS_REGANVIL_OK ||
        mxs_reganvil_peek(&model, UINT32_C(0x10), &pending)
            != MXS_REGANVIL_OK ||
        pending != UINT32_C(0x02)) {
        return EXIT_FAILURE;
    }
    puts("PASS: acknowledging flag A preserved pending flag B.");
    return EXIT_SUCCESS;
}
```

```sh
cc -std=c99 -Wall -Wextra -Wpedantic -Iinclude src/mxs_reganvil.c guide_w1c.c -o guide_w1c
./guide_w1c
```

Change the submitted write to `0x03` and the same test fails. For your own driver, replace the direct write with its acknowledgement function and retain the final expectation.

This example uses an entirely W1C register. A register mixing RW and W1C fields needs a write value that also preserves the intended RW settings. Follow the datasheet. The bundled [W1C demonstration](../examples/interrupt_ack.c) checks both the deliberate bug and its correction, including access traces.

## Test read-clear register side effects

A read-clear bit is returned by the current read and then cleared in the stored register. Even an extra diagnostic read can therefore change what the driver sees next. `mxs_reganvil_peek` observes state without clearing bits or recording an access; use it for test assertions that should not act like driver reads.

| Operation | Observed value | Stored value afterward |
| --- | --- | --- |
| Peek | `0x03` | `0x03` |
| First read | `0x03` | `0x02` |
| Peek | `0x02` | `0x02` |
| Second read | `0x02` | `0x02` |

Save as `guide_read_clear.c`:

```c
#include "mxs_reganvil.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const mxs_reganvil_descriptor registers[] = {
        {
            .offset = UINT32_C(0x08),
            .reset_value = UINT32_C(0x03),
            .ro_mask = UINT32_C(0x03),
            .read_clear_mask = UINT32_C(0x01)
        }
    };
    mxs_reganvil_context model;
    uint32_t state[1];
    uint32_t before, first, after, second;

    if (mxs_reganvil_init(&model, registers, 1, state, 1, NULL, 0)
            != MXS_REGANVIL_OK ||
        mxs_reganvil_peek(&model, UINT32_C(0x08), &before)
            != MXS_REGANVIL_OK ||
        mxs_reganvil_read(&model, UINT32_C(0x08), &first)
            != MXS_REGANVIL_OK ||
        mxs_reganvil_peek(&model, UINT32_C(0x08), &after)
            != MXS_REGANVIL_OK ||
        mxs_reganvil_read(&model, UINT32_C(0x08), &second)
            != MXS_REGANVIL_OK ||
        before != UINT32_C(0x03) || first != UINT32_C(0x03) ||
        after != UINT32_C(0x02) || second != UINT32_C(0x02)) {
        return EXIT_FAILURE;
    }
    puts("PASS: read-clear returned the old value and then cleared the bit.");
    return EXIT_SUCCESS;
}
```

```sh
cc -std=c99 -Wall -Wextra -Wpedantic -Iinclude src/mxs_reganvil.c guide_read_clear.c -o guide_read_clear
./guide_read_clear
```

Read-only and read-clear are compatible: software writes cannot change the bit, while reads have a clearing side effect. The read-clear mask may overlap RW, RO, or W1C fields but must contain only implemented bits. In a driver test, also inspect the optional trace for unexpected extra reads or an incorrect access order.

## Frequently asked questions

### When is RegAnvil a good fit?

Use it when native C tests need persistent register state and RW, RO, W1C, reserved-bit, or read-clear rules. Typical cases include interrupt acknowledgement, status handling, and configuration writes through replaceable accessors.

### How does it differ from function mocks?

A function mock can expect calls and return scripted values. RegAnvil applies declared bit rules across successive operations on stored state. Both approaches can be used in the same test suite.

### Does it intercept existing volatile pointers?

No. Use replaceable accessors, callbacks, or test-build macros. Production accessors use the device's MMIO addresses; test accessors translate them to model offsets.

### Does it emulate the processor or the whole device?

No. RegAnvil models the register rules you supply. Tests explicitly schedule hardware changes between operations. CPU execution, buses, electrical timing, interrupt routing, and complete microcontrollers are outside its scope.

### Do passing tests prove the model matches hardware?

No. Check offsets, masks, reset values, and side effects against the datasheet. Initialization checks internal consistency, not the hardware specification.

### Can simulated hardware change read-only bits?

Yes. Hardware updates are independent of software permissions. Their set and clear masks must be disjoint and contain only implemented bits.

### Does it need a heap or an embedded SDK?

No. The caller supplies descriptors, state, context, and the optional trace buffer. The core is one header and one C file with no MCU SDK dependency.

### Can multiple threads share a model?

Operations on one instance are single-threaded and non-reentrant. The caller must synchronize shared access. Independent instances do not share mutable state.

### What happens when the trace fills?

Register operations continue. Earlier records remain available; later events are dropped and counted. Trace overflow does not turn a successful register access into an error.

### Which platforms are verified?

See the dated [verification record](../VERIFYING.md) and [CI results for your commit](https://github.com/MeysamResan/mxs-reganvil/actions/workflows/ci.yml). Those native executions do not establish physical-hardware validation.
