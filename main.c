/* Environment includes. */
#include "DriverLib.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "hw_memmap.h"
#include "portable.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "uart.h"

#define OLED_WIDTH 96
#define OLED_HEIGHT 16
#define MAX_FILTER_SIZE 50

/* Delay between cycles of the 'sensor' task. */
#define mainSENSOR_DELAY ((TickType_t)100 / portTICK_PERIOD_MS)

/* Delay between cycles of the 'monitor' task. */
#define mainMONITOR_DELAY ((TickType_t)1000 / portTICK_PERIOD_MS)

/* Task priorities. */
#define mainSENSOR_TASK_PRIORITY (tskIDLE_PRIORITY + 3)

/* UART configuration - note this does not use the FIFO so is not very
efficient. */
#define mainBAUD_RATE (19200)

/* Function prototypes */
static void prvSetupHardware(void);
void vCreateQueues(void);
void vCreateTasks(void);
void vIntToString(int value, char *string);
void vSetupHighFrequencyTimer(void);
static void vSensorTask(void *pvParameters);
static void vFilterTask(void *pvParameters);
static void vGraficarTask(void *pvParameters);
static void vMonitorTask(void *pvParameters);
void vSendStringToUart(const char *string);
void vPrintSystemStats(unsigned long uxArraySize, TaskStatus_t *pxTaskStatusArray);
int vUpdateN(int N);
void addValueToSignal(unsigned char image[OLED_WIDTH * 2], int value);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

/* Queues used to communicate between tasks. */
QueueHandle_t xFilterGraficarQueue;
QueueHandle_t xSensorFilterQueue;
QueueHandle_t xUartFilterQueue;

/*-----------------------------------------------------------*/

/**
 * @brief Main function. Initializes hardware, creates queues and tasks, and starts the scheduler.
 * @return int 0 if the function completes successfully.
 */
int main(void) {
  /* Configure the clocks, UART and GPIO. */
  prvSetupHardware();

  vCreateTasks();

  /* Start the scheduler. */
  vTaskStartScheduler();

  /* Will only get here if there was insufficient heap to start the scheduler. */
  return 0;
}

/*-----------------------------------------------------------*/

/**
 * @brief Configures the clock, OLED display, timer and UART.
 */
static void prvSetupHardware(void) {
  /* Setup the PLL. */
  SysCtlClockSet(SYSCTL_SYSDIV_10 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_6MHZ);

  /* Configure the high frequency interrupt used to measure CPU usage. */
  vSetupHighFrequencyTimer();

  /* Initialise the LCD. */
  OSRAMInit(false);
  OSRAMStringDraw("ICOM SO II", 0, 0);
  OSRAMStringDraw("TP4 LM3S811", 16, 1);

  /* Enable the UART. */
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

  /* Configure the UART for 8-N-1 operation. */
  UARTConfigSet(UART0_BASE, mainBAUD_RATE, UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE);
}

/*-----------------------------------------------------------*/

/**
 * @brief Creates the tasks and the message queues.
 */
void vCreateTasks(void) {
  /* Create the queues used by the tasks. */
  vCreateQueues();

  /* Start the tasks defined within the file. */
  xTaskCreate(vSensorTask, "Sensor", configMINIMAL_STACK_SIZE, NULL, mainSENSOR_TASK_PRIORITY, NULL);

  xTaskCreate(vFilterTask, "Filter", configMINIMAL_STACK_SIZE, NULL, mainSENSOR_TASK_PRIORITY - 1, NULL);

  xTaskCreate(vGraficarTask, "Grafic", configMINIMAL_STACK_SIZE, NULL, mainSENSOR_TASK_PRIORITY - 1, NULL);

  xTaskCreate(vMonitorTask, "Monitor", configMINIMAL_STACK_SIZE, NULL, mainSENSOR_TASK_PRIORITY - 2, NULL);
}

/*-----------------------------------------------------------*/

/**
 * @brief Creates the queues used by the tasks.
 */
void vCreateQueues(void) {
  xFilterGraficarQueue = xQueueCreate(10, sizeof(int));
  xSensorFilterQueue = xQueueCreate(10, sizeof(int));
  xUartFilterQueue = xQueueCreate(10, sizeof(int));

  if (xFilterGraficarQueue == NULL || xSensorFilterQueue == NULL || xUartFilterQueue == NULL) {
    /* One or more queues were not created successfully as there was not enough heap memory available. */
    for (;;);
  }
}

/*-----------------------------------------------------------*/

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
    for (;;);
  }

  for (;;) {
    vTaskDelayUntil(&xLastExecutionTime, mainMONITOR_DELAY);
    vPrintSystemStats(uxArraySize, pxTaskStatusArray);
  }
}

/**
 * @brief Prints the system stats to UART.
 * @param uxArraySize Size of the task status array.
 * @param pxTaskStatusArray Array of task statuses.
 */
void vPrintSystemStats(unsigned long uxArraySize, TaskStatus_t *pxTaskStatusArray) {
  volatile UBaseType_t x;
  unsigned int ulTotalRunTime, ulStatsAsPercentage;
  char temp[10] = "";

  vSendStringToUart("\x1B[2J\x1B[H"); // ANSI command to clear screen
  vSendStringToUart("--------- System Monitor ---------\r\n");
  vSendStringToUart("Task\tCPU %\tStatus\tStack HighWaterMark\r\n");

  uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);
  ulTotalRunTime /= 100;

  for (x = 0; x < uxArraySize; x++) {
    vSendStringToUart(pxTaskStatusArray[x].pcTaskName);
    vSendStringToUart("\t");

    if (ulTotalRunTime > 0) {
      ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;
      if (ulStatsAsPercentage == 0) {
        vSendStringToUart("<1");
      } else {
        vIntToString(ulStatsAsPercentage, temp);
        vSendStringToUart(temp);
      }
    } else {
      vSendStringToUart("-");
    }

    vSendStringToUart("\t");

    switch (pxTaskStatusArray[x].eCurrentState) {
    case eRunning:
      vSendStringToUart("Running");
      break;
    case eReady:
      vSendStringToUart("Ready");
      break;
    case eBlocked:
      vSendStringToUart("Blocked");
      break;
    case eSuspended:
      vSendStringToUart("Suspended");
      break;
    case eDeleted:
      vSendStringToUart("Deleted");
      break;
    case eInvalid:
      vSendStringToUart("Invalid");
      break;
    }

    vSendStringToUart("\t");
    vIntToString(pxTaskStatusArray[x].usStackHighWaterMark, temp);
    vSendStringToUart(temp);
    vSendStringToUart("\r\n");
  }
}

/**
 * @brief Sends a string to the UART.
 * @param string Pointer to the null terminated string to send.
 */
void vSendStringToUart(const char *string) {
  while (*string != '\0') {
    UARTCharPut(UART0_BASE, *string);
    string++;
  }
}

/*-----------------------------------------------------------*/

/**
 * @brief Reads sensor data and sends it to the filter task.
 * @param pvParameters unused.
 */
static void vSensorTask(void *pvParameters) {
  TickType_t xLastExecutionTime;

  /* Initialise xLastExecutionTime so the first call to vTaskDelayUntil() works correctly. */
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

/*-----------------------------------------------------------*/

/**
 * @brief Filters the sensor data and sends the average value to the graficar task.
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

/*-----------------------------------------------------------*/

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

/*-----------------------------------------------------------*/

/**
 * @brief Converts an integer to a string.
 * @param value The integer value.
 * @param string The string buffer to hold the converted value.
 */
void vIntToString(int value, char *string) {
  int i = 0;
  if (value == 0) {
    string[i++] = '0';
  }

  while (value != 0) {
    string[i++] = value % 10 + '0';
    value = value / 10;
  }

  // reverse string
  for (int j = 0; j < i / 2; j++) {
    char temp = string[j];
    string[j] = string[i - j - 1];
    string[i - j - 1] = temp;
  }

  string[i] = '\0';
}

/*-----------------------------------------------------------*/

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
