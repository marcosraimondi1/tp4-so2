# TP4: FreeRTOS

## Requirements
- [FreeRTOS](https://www.freertos.org/)
- arm-none-eabi-gcc : cross compiler toolchain for ARM
- qemu
- bear (for compile_commands.json generation)


## Build
```sh
make clean
make
# bear -- make 
```

## Running
```sh
qemu-system-arm -M lm3s811evb -kernel gcc/RTOSDemo.axf -serial stdio
```

## Debugging
- Run qemu with gdb server enabled and an entrypoint breakpoint
```sh
qemu-system-arm -M lm3s811evb -kernel gcc/RTOSDemo.axf -S -s -serial stdio
```

- Run gdb and connect to gdb server
```sh
arm-none-eabi-gdb gcc/RTOSDemo.axf -tui
(gdb) target remote localhost:1234
(gdb) break main
```

