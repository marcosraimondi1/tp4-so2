/* Environment includes. */
#include "DriverLib.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "portable.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "uart.h"

/* Delay between cycles of the 'sensor' task. */
#define mainSENSOR_DELAY ((TickType_t)100 / portTICK_PERIOD_MS)

/* Delay between cycles of the 'monitor' task. */
#define mainMONITOR_DELAY ((TickType_t)1000 / portTICK_PERIOD_MS)

/* Task priorities. */
#define mainSENSOR_TASK_PRIORITY (tskIDLE_PRIORITY + 3)

/* Misc. */
#define mainQUEUE_SIZE (10)
#define mainNO_DELAY ((TickType_t)0)

/* UART configuration - note this does not use the FIFO so is not very
efficient. */
#define mainBAUD_RATE (19200)
#define mainFIFO_SET (0x10)

/* Configure the processor and peripherals */
static void prvSetupHardware(void);

/* Create queues used by the tasks. */
void vCreateQueues(void);

/* Create tasks. */
void vCreateTasks(void);

/* convert integer to string. */
void vIntToString(int value, char *string);

/*-----------------------------------------------------------*/

int main(void) {
  /* Configure the clocks, UART and GPIO. */
  prvSetupHardware();

  vCreateQueues();

  vCreateTasks();

  /* Start the scheduler. */
  vTaskStartScheduler();

  /* Will only get here if there was insufficient heap to start the scheduler.
   */

  return 0;
}

/*-----------------------------------------------------------*/

static void prvSetupHardware(void) {
  /* Setup the PLL. */
  SysCtlClockSet(SYSCTL_SYSDIV_10 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                 SYSCTL_XTAL_6MHZ);

  /* Initialise the LCD> */
  OSRAMInit(false);
  OSRAMStringDraw("ICOM SO II", 0, 0);
  OSRAMStringDraw("TP4 LM3S811", 16, 1);

  /* Enable the UART.  */
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

  /* Configure the UART for 8-N-1 operation. */
  UARTConfigSet(UART0_BASE, mainBAUD_RATE,
                UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE |
                    UART_CONFIG_STOP_ONE);
}

/*-----------------------------------------------------------*/

/* Queues used to communicate between tasks. */
QueueHandle_t xFilterGraficarQueue;
QueueHandle_t xSensorFilterQueue;
QueueHandle_t xUartFilterQueue;

void vCreateQueues(void) {
  xFilterGraficarQueue = xQueueCreate(10, sizeof(int));
  xSensorFilterQueue = xQueueCreate(10, sizeof(int));
  xUartFilterQueue = xQueueCreate(10, sizeof(int));

  if (xFilterGraficarQueue == NULL || xSensorFilterQueue == NULL ||
      xUartFilterQueue == NULL) {
    /* One or more queues were not created successfully as there was not
    enough heap memory available. */
    for (;;)
      ;
  }
}

/*-----------------------------------------------------------*/

/* Tasks to schedule. */
static void vSensorTask(void *pvParameters);
static void vFilterTask(void *pvParameters);
static void vGraficarTask(void *pvParameter);
static void vMonitorTask(void *pvParameter);

void vCreateTasks(void) {
  /* Start the tasks defined within the file. */
  xTaskCreate(vSensorTask, "Sensor", configMINIMAL_STACK_SIZE, NULL,
              mainSENSOR_TASK_PRIORITY, NULL);

  xTaskCreate(vFilterTask, "Filter", configMINIMAL_STACK_SIZE, NULL,
              mainSENSOR_TASK_PRIORITY - 1, NULL);

  xTaskCreate(vGraficarTask, "Grafic", configMINIMAL_STACK_SIZE, NULL,
              mainSENSOR_TASK_PRIORITY - 1, NULL);

  xTaskCreate(vMonitorTask, "Monitor", configMINIMAL_STACK_SIZE * 4, NULL,
              mainSENSOR_TASK_PRIORITY - 2, NULL);
}

/*-----------------------------------------------------------*/
void vSendStringToUart(const char *string);

// https://www.freertos.org/uxTaskGetSystemState.html
static void vMonitorTask(void *pvParameter) {
  TickType_t xLastExecutionTime;
  xLastExecutionTime = xTaskGetTickCount();

  TaskStatus_t *pxTaskStatusArray;
  volatile UBaseType_t uxArraySize, x;
  unsigned int ulTotalRunTime, ulStatsAsPercentage;
  uxArraySize = uxTaskGetNumberOfTasks();
  pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
  if (pxTaskStatusArray == NULL) {
    for (;;)
      ;
  }

  for (;;) {
    vTaskDelayUntil(&xLastExecutionTime, mainMONITOR_DELAY);

    vSendStringToUart("\x1B[2J\x1B[H"); // ANSI command to clear screen

    vSendStringToUart("--------- System Monitor ---------\r\n");
    vSendStringToUart("Task\tCPU\tStatus\tStack HighWaterMark\r\n");

    uxArraySize =
        uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

    ulTotalRunTime /= 100; // convert to percentage

    for (x = 0; x < uxArraySize; x++) {

      vSendStringToUart(pxTaskStatusArray[x].pcTaskName);
      vSendStringToUart("\t");

      if (ulTotalRunTime > 0) {
        char temp[10] = "";
        ulStatsAsPercentage =
            pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;
        vIntToString(ulStatsAsPercentage, temp);
        vSendStringToUart(temp);
      } else {
        vSendStringToUart("0");
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
      char temp[10] = "";
      vIntToString(pxTaskStatusArray[x].usStackHighWaterMark, temp);
      vSendStringToUart(temp);
      vSendStringToUart("\r\n");
    }
  }
}

void vSendStringToUart(const char *string) {
  while (*string != '\0') {
    UARTCharPut(UART0_BASE, *string);
    string++;
  }
}

/*-----------------------------------------------------------*/

static void vSensorTask(void *pvParameters) {
  TickType_t xLastExecutionTime;

  /* Initialise xLastExecutionTime so the first call to vTaskDelayUntil()
  works correctly. */
  xLastExecutionTime = xTaskGetTickCount();

  static int temp = 0;
  static int dir = 1;

  for (;;) {
    /* Perform this check every mainSENSOR_DELAY milliseconds. */
    vTaskDelayUntil(&xLastExecutionTime, mainSENSOR_DELAY);

    /* Update sensor temperature reading. */
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

static void vFilterTask(void *pvParameters) {
  static int N = 1;
  static int sum = 0;
  static int last_value = 0;

  for (;;) {
    int values[N] = {};

    /* Wait for a message to arrive. */
    xQueueReceive(xSensorFilterQueue, &last_value, portMAX_DELAY);

    /* Shift values */
    for (int i = N - 1; i > 0; i--) {
      values[i] = values[i - 1];
    }
    values[0] = last_value;

    /* Calculate average */
    sum = 0;
    for (int i = 0; i < N; i++) {
      sum += values[i];
    }
    sum = sum / N;

    /* Send average to graficar task. */
    xQueueSend(xFilterGraficarQueue, &sum, portMAX_DELAY);
  }
}

/*-----------------------------------------------------------*/

#define OLED_WIDTH 96
#define OLED_HEIGHT 16

void addValueToSignal(unsigned char image[OLED_WIDTH * 2], int value);

static void vGraficarTask(void *pvParameters) {
  static unsigned char signal[OLED_WIDTH * 2] = {0};
  static int value = 0;

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

void addValueToSignal(unsigned char image[OLED_WIDTH * 2], int value) {
  // shift signal
  for (int i = OLED_WIDTH - 1; i > 0; i--) {
    image[i] = image[i - 1];
    image[i + OLED_WIDTH] = image[i - 1 + OLED_WIDTH];
  }

  image[OLED_WIDTH] = 0;
  image[0] = 0;

  // add new value
  if (value < 8) {
    image[OLED_WIDTH] = (1 << (7 - value));
  } else {
    image[0] = (1 << (15 - value));
  }
}

/*-----------------------------------------------------------*/

void vUART_ISR(void) {
  unsigned long ulStatus;

  /* What caused the interrupt. */
  ulStatus = UARTIntStatus(UART0_BASE, pdTRUE);

  /* Clear the interrupt. */
  UARTIntClear(UART0_BASE, ulStatus);
}

/*-----------------------------------------------------------*/

void vGPIO_ISR(void) {}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  for (;;)
    ;
}

/*-----------------------------------------------------------*/

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
