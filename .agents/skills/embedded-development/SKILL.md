# Embedded Development

## Purpose

Provide practical guidance for developing, debugging, reviewing, and modifying embedded software.

This skill applies primarily to:

* ESP32 and similar microcontrollers
* C and C++
* Arduino-based firmware
* ESP-IDF
* FreeRTOS
* Bare-metal or RTOS-based embedded systems
* Sensors and peripherals
* I2C, SPI, UART, GPIO
* PWM and timers
* Non-volatile storage
* Networking-capable embedded devices

The objective is to produce firmware that is:

* Correct
* Deterministic where practical
* Resource-conscious
* Maintainable
* Testable
* Robust against hardware and communication failures

Do not introduce unnecessary complexity or rewrite working firmware without a clear reason.

---

# 1. Analyze Before Modifying

Before changing embedded code:

1. Inspect the project structure.
2. Identify the MCU/module and framework.
3. Locate the initialization sequence.
4. Locate the main execution flow.
5. Identify tasks, interrupts, callbacks, and timers.
6. Identify shared resources.
7. Search for existing implementations.
8. Inspect relevant hardware definitions.
9. Check how configuration and persistent state are stored.
10. Identify dependencies on external devices.

Do not start by rewriting the requested function.

First determine how the existing firmware currently works.

---

# 2. Preserve Existing Architecture

When modifying existing firmware:

* Prefer minimal changes.
* Preserve existing APIs where possible.
* Preserve existing naming conventions.
* Preserve established state machines.
* Preserve existing error handling unless it is demonstrably incorrect.
* Do not introduce a new framework or abstraction without justification.

If the existing architecture is imperfect but functional, solve the requested problem without unnecessarily redesigning the entire system.

If an architectural change is genuinely necessary, explain:

1. Current limitation.
2. Why the current design cannot support the requirement.
3. Proposed design.
4. Migration impact.

---

# 3. Hardware Awareness

Firmware is coupled to physical hardware.

Before modifying hardware-dependent code, identify:

* MCU/module
* GPIO assignments
* supply voltage
* peripheral voltage
* pull-up/pull-down requirements
* communication buses
* device addresses
* interrupt pins
* reset pins
* enable pins
* boot/programming pins
* timing requirements

Do not assume that two components with similar names or functions are electrically or functionally interchangeable.

If hardware information is missing, inspect the repository for:

* schematics
* pin definitions
* board configuration
* datasheets
* documentation
* BOM
* existing initialization code

If it cannot be determined, explicitly state the assumption.

---

# 4. Resource Constraints

Embedded systems have limited resources.

Always consider:

* RAM
* flash
* heap
* stack
* CPU time
* power consumption
* communication bandwidth
* persistent-storage lifetime

Avoid unnecessary:

* dynamic memory allocation
* large buffers
* duplicated data
* string copies
* logging
* polling loops
* blocking operations

Do not optimize prematurely, but do not ignore resource constraints.

When a resource-intensive approach is proposed, explain its impact.

---

# 5. Memory Management

Prefer deterministic memory usage where practical.

Be cautious with:

```cpp
malloc()
calloc()
realloc()
free()
new
delete
```

especially inside:

* high-frequency loops
* FreeRTOS tasks
* callbacks
* long-running services

Consider:

* heap fragmentation
* allocation failures
* object lifetime
* ownership
* buffer size
* stack usage

Do not introduce dynamic allocation merely for convenience when a fixed-size or statically allocated solution is sufficient.

---

# 6. Strings and Buffers

Embedded string handling requires special care.

Before modifying a buffer, determine:

* capacity
* actual length
* ownership
* lifetime
* null termination
* encoding assumptions

Avoid unsafe operations such as blindly copying data into fixed-size buffers.

When handling external input, validate its length before copying or parsing.

For protocol payloads, distinguish between:

```text
buffer capacity
payload length
string length
```

These are not necessarily the same.

---

# 7. FreeRTOS

When FreeRTOS is used, understand the concurrency model before modifying code.

For each task, identify:

* task name
* priority
* stack size
* execution frequency
* blocking operations
* shared resources
* synchronization mechanisms

Common synchronization mechanisms include:

* Mutex
* Binary semaphore
* Counting semaphore
* Queue
* Event group
* Task notification
* Software timer

Choose the mechanism according to the ownership and communication problem.

Do not use a global variable as an implicit synchronization mechanism.

---

# 8. Shared Resources

Whenever multiple tasks access the same resource, evaluate synchronization.

Examples:

* shared sensor data
* configuration structures
* network clients
* I2C bus
* SPI bus
* filesystem
* NVS/Preferences
* serial interfaces

A mutex should protect a clearly defined resource.

Avoid unnecessarily holding a mutex during:

* network operations
* long calculations
* delays
* blocking I/O

Keep critical sections as short as practical.

---

# 9. Task Design

A FreeRTOS task should have a clear responsibility.

Prefer:

```text
Task
 ├── wait
 ├── perform operation
 ├── update state
 └── wait again
```

over continuously executing:

```cpp
while (true) {
    // perform everything
}
```

For periodic work, prefer appropriate timing mechanisms rather than uncontrolled loops.

Consider:

* task priority
* execution period
* jitter
* blocking time
* watchdog interaction

Do not increase task priority simply to "make it faster."

---

# 10. Delays and Blocking

Before using:

```cpp
delay()
```

or a blocking operation, determine whether the code is:

* Arduino loop
* FreeRTOS task
* interrupt
* callback
* timer callback

In FreeRTOS tasks, prefer RTOS-aware timing mechanisms when appropriate.

Never perform long blocking operations inside an interrupt service routine.

Avoid blocking operations inside timing-critical code.

---

# 11. Interrupts

Interrupt service routines should remain short and deterministic.

Avoid performing the following inside an ISR unless the platform explicitly supports and the design requires it:

* long calculations
* network operations
* blocking operations
* dynamic memory allocation
* filesystem access
* extensive logging

When an interrupt needs to notify application code, consider:

* task notification
* semaphore
* queue
* event mechanism

The ISR should generally signal work rather than perform the complete operation.

---

# 12. GPIO

When working with GPIO:

Verify:

* input/output mode
* pull-up/pull-down
* active-high/active-low behavior
* voltage compatibility
* bootstrapping implications
* interrupt configuration

Do not assume that a GPIO is safe to use simply because it is exposed on the module.

Some MCU pins have special behavior during boot or reset.

---

# 13. I2C

Before modifying I2C code, verify:

* SDA pin
* SCL pin
* device address
* bus frequency
* pull-up resistors
* supply voltage
* bus ownership
* timeout behavior

Consider bus failures such as:

* device disconnected
* incorrect address
* stuck SDA/SCL
* missing pull-ups
* electrical noise
* incorrect voltage

Do not assume that a successful initialization means the peripheral is functioning correctly.

---

# 14. SPI

For SPI devices, verify:

* MOSI
* MISO
* SCLK
* CS
* clock frequency
* SPI mode
* bit order
* transaction boundaries

Different SPI devices may require different configuration.

If multiple devices share the bus, verify chip-select behavior and transaction ownership.

---

# 15. UART / Serial

For UART communication, verify:

* TX
* RX
* baud rate
* data bits
* parity
* stop bits
* flow control
* message framing

Do not assume that a serial stream is message-oriented.

If messages have a protocol, explicitly define how message boundaries are detected.

Possible approaches include:

* fixed length
* delimiter
* length field
* packet framing
* timeout

---

# 16. Timers and PWM

When using timers or PWM, identify:

* frequency
* resolution
* duty cycle
* timer/channel ownership
* hardware limitations

Do not assume that changing frequency has no effect on resolution or available channels.

For LED or actuator control, distinguish between:

```text
desired output
actual hardware output
```

and consider initialization and fail-safe states.

---

# 17. RTC and Time

Treat time handling as a subsystem.

Separate:

1. RTC communication
2. Time validation
3. System clock
4. Time synchronization
5. Application scheduling
6. Elapsed-time calculations

Do not mix these responsibilities unnecessarily.

Validate date/time input before applying it.

For example:

```text
seconds: 0–59
minutes: 0–59
hours:   0–23
```

Also validate:

* day
* month
* year

When calculating elapsed days, define the convention explicitly.

For example:

```text
elapsed_days = current_date - start_date
```

Do not silently mix calendar-day calculations with elapsed seconds.

---

# 18. State Machines and Enums

When firmware uses states or enums:

Understand whether enum values are used for:

* array indexes
* persistent storage
* communication protocols
* database fields
* configuration serialization
* bit masks

Before changing enum order or numeric values:

1. Search all references.
2. Check persistent storage.
3. Check serialized data.
4. Check communication interfaces.
5. Check external consumers.

Never reorder persistent enum values casually.

---

# 19. Persistent Storage

When working with:

* NVS
* Preferences
* EEPROM
* flash storage
* configuration files

distinguish between:

```text
factory defaults
runtime state
configuration
credentials
firmware metadata
```

Consider:

* missing keys
* corrupted values
* invalid values
* firmware upgrades
* backwards compatibility
* flash write endurance

Do not write persistent values unnecessarily on every loop iteration.

---

# 20. Networking

When firmware communicates with external systems, explicitly define:

```text
transport
endpoint/topic
authentication
payload
timeout
retry behavior
error handling
```

Possible transports include:

* HTTP
* HTTPS
* MQTT
* TCP
* UDP

Do not mix assumptions between protocols.

For constrained devices, consider:

* payload size
* connection overhead
* retry frequency
* timeout duration
* offline behavior
* power consumption

---

# 21. Error Handling

Errors should contain enough context to diagnose the failure.

Prefer:

```text
operation + reason + relevant state
```

instead of:

```text
ERROR
```

Differentiate between:

* recoverable errors
* transient errors
* configuration errors
* hardware errors
* fatal errors

Do not silently ignore errors unless that behavior is intentional.

Avoid retry loops without limits.

---

# 22. Watchdog and Recovery

When the platform provides watchdog functionality, understand what can cause a reset.

Potential causes include:

* deadlock
* infinite loop
* blocking operation
* priority starvation
* long critical section
* hardware failure

Do not "fix" watchdog resets by simply disabling the watchdog.

Find the underlying blocking or scheduling problem.

---

# 23. Logging and Diagnostics

Logs should help diagnose the system without overwhelming the device.

Useful diagnostic information may include:

```text
timestamp
device identifier
task/state
operation
result
error code
```

Avoid excessive logging inside:

* high-frequency loops
* interrupts
* timing-critical sections

For production firmware, consider whether logs should be:

* disabled
* reduced
* configurable
* sent through a diagnostic interface

---

# 24. Firmware Updates

When implementing firmware updates, consider:

```text
current version
        ↓
update check
        ↓
available version
        ↓
download
        ↓
validation
        ↓
installation
        ↓
reboot
```

Consider failure scenarios:

* interrupted download
* corrupted image
* invalid image
* insufficient storage
* power loss
* network failure
* rollback/recovery

Never treat an OTA operation as simply:

```text
download → flash → reboot
```

without considering failure recovery.

---

# 25. Security

Never hard-code:

* passwords
* API keys
* private keys
* authentication secrets
* production credentials

When communicating over a network, consider:

* TLS
* certificate validation
* authentication
* authorization
* device identity
* credential storage

Do not disable certificate validation merely to make development communication work unless the change is explicitly isolated to a development environment.

---

# 26. Testing Strategy

Embedded testing may require multiple levels.

### Unit testing

Use for:

* calculations
* parsing
* validation
* state transitions
* pure functions

### Integration testing

Use for:

* drivers
* communication
* storage
* task interactions

### Hardware testing

Use for:

* GPIO
* I2C
* SPI
* UART
* sensors
* RTC
* Wi-Fi
* Bluetooth
* PWM
* OTA

### Manual validation

Use when hardware or external services cannot reasonably be simulated.

Always distinguish:

```text
Verified
Expected
Not tested
```

Never claim that firmware was compiled, flashed, or physically tested unless it actually was.

---

# 27. Debugging Method

When debugging firmware:

Do not immediately rewrite the function.

Determine:

```text
Expected behavior
        ↓
Actual behavior
        ↓
First point of divergence
        ↓
Root cause
        ↓
Minimal correction
```

Inspect:

* initialization
* state transitions
* timing
* concurrency
* inputs
* outputs
* hardware assumptions
* persistent state

If the problem occurs intermittently, investigate:

* race conditions
* timing
* uninitialized memory
* stack overflow
* heap fragmentation
* watchdog behavior
* communication failures

Do not assume intermittent behavior is random.

---

# 28. Code Quality

Prefer:

* small functions
* explicit ownership
* meaningful names
* clear interfaces
* deterministic behavior
* minimal global state
* explicit error handling

Avoid:

* unnecessary abstractions
* deeply nested conditionals
* duplicated hardware logic
* hidden side effects
* magic numbers
* excessive macros

When a constant has hardware or protocol significance, give it a descriptive name.

---

# 29. Validation After Changes

After modifying embedded code, validate as much as the available environment allows.

Possible validation:

```text
Static inspection
Compile
Unit tests
Integration tests
Hardware-in-the-loop
Serial/log inspection
Manual hardware test
```

Clearly report what was actually performed.

Example:

```text
Validation:
- Code inspection: completed
- Compilation: not performed
- Unit tests: not available
- Hardware test: not performed
```

Never imply that an unperformed hardware test was successful.

---

# 30. Change Reporting

When completing a modification, report:

### Changed

List affected files/functions.

### Root Cause

Explain what caused the original behavior.

### Solution

Explain the modification.

### Validation

State what was actually verified.

### Remaining Risks

Mention only relevant unresolved issues.

Keep the explanation concise and technically specific.
