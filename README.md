# TP4: FreeRTOS

En este trabajo se realiza una implementacion de FreeRTOS en la placa [LM3S811](https://www.ti.com/lit/ug/spmu030b/spmu030b.pdf) (CORTEX-M3). Para ello se utilizara el emulador Qemu.

## Consigna

Utilizando FreeRTOS, implementar las siguientes tareas:
- una tarea que simule un sensor de temperatura cque tome valores con una frecuencia de 10 [HZ].
- otra tarea que reciba los valores del sensor y los filtre con un filtro pasa bajo de ventana deslizante. Un filtro promediador de las ultimas N muestras.
- una tarea que muestre en un el display la senal resultante del filtro.
- una tarea que muestre el estado de las otras tareas (estado de la tarea, porcentaje de la cpu)

El valor de N del filtro promediador debe poder ser ajustado a traves de comandos enviados por UART.

## FreeRTOS

FreeRTOS es un sistema operativo en tiempo real (RTOS, por sus siglas en inglés) ampliamente utilizado en sistemas embebidos. Fue desarrollado por Richard Barry y es mantenido por Amazon Web Services (AWS). FreeRTOS es conocido por su simplicidad, portabilidad y tamaño reducido, lo que lo hace ideal para dispositivos con recursos limitados, como microcontroladores y pequeños sistemas integrados.

## QEMU

QEMU (Quick EMUlator) es un emulador y virtualizador de código abierto que permite ejecutar programas y sistemas operativos compilados para una arquitectura de hardware diferente a la del anfitrión. QEMU es conocido por su flexibilidad y versatilidad, lo que lo convierte en una herramienta valiosa tanto para desarrolladores como para investigadores. QEMU puede emular una amplia variedad de arquitecturas de CPU, incluyendo x86, ARM, PowerPC, SPARC, MIPS, y más. 

## Manejo de Interrupciones

Para poder setear un handler de una interrupcion para crear una ISR (Interrupt Service Routine) custom es necesario registrar la funcion de la ISR en la tabla de interrupciones en el archivo [init/startup.c](./init/startup.c).

## Requerimientos

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

- Correr qemu con gdb server habilitado y un breakpoint en main:
```sh
qemu-system-arm -M lm3s811evb -kernel gcc/RTOSDemo.axf -S -s -serial stdio
```

- Correr gdb y conectarse a gdb server
```sh
arm-none-eabi-gdb gcc/RTOSDemo.axf -tui
(gdb) target remote localhost:1234
(gdb) break main
```

## Referencias
- [FreeRTOS](https://www.freertos.org/)
- [LM3S811](https://www.ti.com/lit/ug/spmu030b/spmu030b.pdf)

