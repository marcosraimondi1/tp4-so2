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

## Sensor de Temperatura

Se simula un sensor de temperatura que genera valores cada 0.1 segundos, es decir, 10 Heartz. Se genera una senal triangular ya que es facil de identificar posteriormente con el filtrado. La tarea tiene un delay donde se bloquea, para posteriormente despertar y enviar el siguiente valor en una cola de mensajes que comunica con la tarea del filtro.

```C
/**
 * @brief Reads sensor data and sends it to the filter task.
 * @param pvParameters unused.
 */
static void vSensorTask(void *pvParameters) {
  TickType_t xLastExecutionTime;

  /* Initialise xLastExecutionTime so the first call to vTaskDelayUntil() works
   * correctly. */
  xLastExecutionTime = xTaskGetTickCount();

  int temp = 0;
  int dir = 1;

  for (;;) {
    /* Perform this check every mainSENSOR_DELAY milliseconds. */
    vTaskDelayUntil(&xLastExecutionTime, mainSENSOR_DELAY);

    /* Update sensor temperature reading. Triangular signal */
    temp = temp + (2 * dir);
    if (temp >= 15) {
      dir = -1;
      temp = 15;
    } else if (temp <= 0) {
      dir = 1;
      temp = 0;
    }

    /* Send temperature to filter task. */
    xQueueSend(xSensorFilterQueue, &temp, portMAX_DELAY);
  }
}
```

## Filtrado

Aqui, la tarea espera por valores generados por el sensor y se filtran los ultimos N valores. Al llegar un valor, se rotan los valores anteriores del arreglo como un shiftregister, descartando el ultimo valor e ingresando el nuevo. Posteriormente se calcula el promedio. El valor de N puede cambiar (hasta un maximo de MAX_FILTER_SIZE). Finalmente se envia el valor del promedio a la tarea del grafico a traves de otra cola de mensajes. 

```C
/**
 * @brief Filters the sensor data and sends the average value to the graficar
 * task.
 * @param pvParameters unused.
 */
static void vFilterTask(void *pvParameters) {
  static int values[MAX_FILTER_SIZE] = {0};
  int N = 1;
  int sum = 0;
  int last_value = 0;

  for (;;) {
    /* Wait for a message to arrive. */
    xQueueReceive(xSensorFilterQueue, &last_value, portMAX_DELAY);

    N = vUpdateN(N);

    /* Shift values */
    for (int i = MAX_FILTER_SIZE - 1; i > 0; i--) {
      values[i] = values[i - 1];
    }
    values[0] = last_value;

    /* Calculate average - only use N values in filter */
    sum = 0;
    for (int i = 0; i < N; i++) {
      sum += values[i];
    }
    sum = sum / N;

    /* Send average to graficar task. */
    xQueueSend(xFilterGraficarQueue, &sum, portMAX_DELAY);
  }
}

```

### Cambio de N por UART

El valor de N se actualiza leyendo comandos por UART, si se envia el caracter 'u' se aumenta el valor de N, caso contrario si se envia 'd' se decrementa.
```C
/**
 * @brief Updates the filter size N based on UART commands.
 * @param currentN The current filter size.
 * @return int The updated filter size.
 */
int vUpdateN(int currentN) {
  while (UARTCharsAvail(UART0_BASE)) {
    char cmd = UARTCharGet(UART0_BASE);
    if (cmd == 'u') {
      currentN++;
      if (currentN > MAX_FILTER_SIZE) {
        currentN = MAX_FILTER_SIZE;
      }
    } else if (cmd == 'd') {
      currentN--;
      if (currentN < 1) {
        currentN = 1;
      }
    }
  }
  return currentN;
}
```
## Graficando en el display 
En la tarea de graficado, se espera por nuevos valores del filtro y se actualiza el display:
```C
/**
 * @brief Receives filtered data and displays it on the OLED.
 * @param pvParameters unused
 */
static void vGraficarTask(void *pvParameters) {
  static unsigned char signal[OLED_WIDTH * 2] = {0};
  int value = 0;

  OSRAMClear();
  addValueToSignal(signal, 0);
  OSRAMImageDraw(signal, 0, 0, OLED_WIDTH, 2);

  for (;;) {
    /* Wait for a message to arrive. */
    xQueueReceive(xFilterGraficarQueue, &value, portMAX_DELAY);

    /* Write the image to the LCD. */
    addValueToSignal(signal, value);
    OSRAMImageDraw(signal, 0, 0, OLED_WIDTH, 2);
  }
}
```
Para graficar la senal en el display OLED, se tiene en cuenta el mapeo de pixeles y bits de la pantalla:

    +-------+  +-------+  +-------+  +-------+
    |   | 0 |  |   | 0 |  |   | 0 |  |   | 0 |
    | B | 1 |  | B | 1 |  | B | 1 |  | B | 1 |
    | y | 2 |  | y | 2 |  | y | 2 |  | y | 2 |
    | t | 3 |  | t | 3 |  | t | 3 |  | t | 3 |
    | e | 4 |  | e | 4 |  | e | 4 |  | e | 4 |
    |   | 5 |  |   | 5 |  |   | 5 |  |   | 5 |
    | 0 | 6 |  | 1 | 6 |  | 2 | 6 |  | 3 | 6 |
    |   | 7 |  |   | 7 |  |   | 7 |  |   | 7 |
    +-------+  +-------+  +-------+  +-------+
                                              
    +-------+  +-------+  +-------+  +-------+
    |   | 0 |  |   | 0 |  |   | 0 |  |   | 0 |
    | B | 1 |  | B | 1 |  | B | 1 |  | B | 1 |
    | y | 2 |  | y | 2 |  | y | 2 |  | y | 2 |
    | t | 3 |  | t | 3 |  | t | 3 |  | t | 3 |
    | e | 4 |  | e | 4 |  | e | 4 |  | e | 4 |
    |   | 5 |  |   | 5 |  |   | 5 |  |   | 5 |
    | 4 | 6 |  | 5 | 6 |  | 6 | 6 |  | 7 | 6 |
    |   | 7 |  |   | 7 |  |   | 7 |  |   | 7 |
    +-------+  +-------+  +-------+  +-------+
Asi se genera un arreglo de OLED_WIDTH*2 chars (8 bits). Cada bit de cada char maneja 8 pixeles en columna. La primera mitad del arreglo hace referencia a la mitad superior del display, y la segunda mitad a la parte inferior.

Esto se hace en la siquiente seccion de codigo donde se agrega un valor al arreglo de la senal, primero avanzando en una posicion los valores anteriores, y posteriormente agregando en la primera columna el nuevo valor a la altura que le corresponde.
```C
/**
 * @brief Adds a value to the OLED signal array and shifts the existing values.
 * @param image The signal array.
 * @param value The value to add.
 */
void addValueToSignal(unsigned char image[OLED_WIDTH * 2], int value) {
  // shift signal
  for (int i = OLED_WIDTH - 1; i > 0; i--) {
    image[i] = image[i - 1];
    image[i + OLED_WIDTH] = image[i - 1 + OLED_WIDTH];
  }

  image[OLED_WIDTH] = 0;
  image[0] = 0;

  // add new value in the correct height position
  if (value < 8) {
    image[OLED_WIDTH] = (1 << (7 - value));
  } else {
    image[0] = (1 << (15 - value));
  }
}
```

![image](https://github.com/marcosraimondi1/tp4-so2/assets/69517496/c33e72a8-7fc5-41a5-96fd-3c86a2d398b0)



## Monitoreo
Esta tarea se ejecuta periodicamente cada cierto tiempo. Para realizarla se siguio el ejemplo en la documentacion de la funcion [uxTaskGetSystemState()](https://www.freertos.org/uxTaskGetSystemState.html). Primero se crea un arreglo con el suficiente espacio para almacenar el estado de las tareas del sistema que se cargara luego con sus estados.

```C
/**
 * @brief Monitors the system and prints system stats.
 * @param pvParameter unused.
 */
static void vMonitorTask(void *pvParameter) {
  TickType_t xLastExecutionTime;
  xLastExecutionTime = xTaskGetTickCount();

  // allocate enough space for every task
  TaskStatus_t *pxTaskStatusArray;
  volatile UBaseType_t uxArraySize;
  uxArraySize = uxTaskGetNumberOfTasks();
  pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
  if (pxTaskStatusArray == NULL) {
    for (;;)
      ;
  }

  for (;;) {
    vTaskDelayUntil(&xLastExecutionTime, mainMONITOR_DELAY);
    vPrintSystemStats(uxArraySize, pxTaskStatusArray);
  }
}
```

Se envia el estado por UART:

![image](https://github.com/marcosraimondi1/tp4-so2/assets/69517496/dd1106e1-4da7-4ea6-a54c-193ac76c73af)

## Calculo del Stack

A cada tarea se le asigna un tamano fijo de stack. Al principio este valor fue sobredimensionado para que no haya stack overflow. Luego con la tarea de monitor se puedo observar el Stack High Water Mark, indica el valor minimo de stack restante que se alcanzo hasta ese momento. Mientras mas cerca de 0 este mas cerca de un stack overflow. Si el valor es cero el stack overflow es inminente. Contando con este valor y utilizando el `vApplicationStackOverflowHook` que es un callback que se ejecuta cuando se detecta un stack overflow, se puede identificar el momento y en que tarea sucedio el stack overflow, y ajustar los valores de stack asignados consecuentemente.

```C
/**
 * @brief Hook function for stack overflow.
 * @param xTask Task handle.
 * @param pcTaskName Task name.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  vSendStringToUart("\nSTACK OVERFLOW on '");
  vSendStringToUart(pcTaskName);
  vSendStringToUart("' task\r\n");
  for (;;) {
  }
}
```
En la documentacion de [FreeRTOS Checking Stack Overflow](https://www.freertos.org/Stacks-and-stack-overflow-checking.html), se indican 2 opciones que se pueden utilizar para detectar el stack overflow y llamar a este callback:
1. El kernel verifica que el puntero de pila del procesador permanezca dentro del espacio de pila válido cuando se realiza un cambio de contexto (cambio de tarea), dado que es probable que la pila alcance su valor mas alto (mas profundo) cuando almacene el contexto de la tarea.
2. Cuando se crea una tarea por primera vez, su pila se llena con un valor conocido. Al cambiar una tarea del estado de Ejecución, el kernel del RTOS puede verificar los últimos 16 bytes dentro del rango de pila válido para asegurarse de que estos valores conocidos no hayan sido sobrescritos por la actividad de la tarea o de la interrupción.

En este caso se utilizo el metodo 2, es un poco menos eficiente pero permite detectar mas casos de stack overflow.

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

