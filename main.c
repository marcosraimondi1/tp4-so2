/* Environment includes. */
#include "DriverLib.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

/* Delay between cycles of the 'sensor' task. */
#define mainSENSOR_DELAY ((TickType_t)100 / portTICK_PERIOD_MS)

/* Task priorities. */
#define mainSENSOR_TASK_PRIORITY (tskIDLE_PRIORITY + 3)

/* Misc. */
#define mainQUEUE_SIZE (10)
#define mainNO_DELAY ((TickType_t)0)

/* Configure the processor and peripherals */
static void prvSetupHardware(void);

/* Create queues used by the tasks. */
void vCreateQueues(void);

/* Create tasks. */
void vCreateTasks(void);

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
static void vGraficarTask(void *pvParameter);

void vCreateTasks(void) {
  /* Start the tasks defined within the file. */
  xTaskCreate(vSensorTask, "Sensor", configMINIMAL_STACK_SIZE, NULL,
              mainSENSOR_TASK_PRIORITY, NULL);

  xTaskCreate(vGraficarTask, "Graficar", configMINIMAL_STACK_SIZE, NULL,
              mainSENSOR_TASK_PRIORITY - 1, NULL);
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
    temp = temp + (1 * dir);
    if (temp == 15) {
      dir = -1;
    } else if (temp == 0) {
      dir = 1;
    }

    /* Send temperature to filter task. */
    xQueueSend(xFilterGraficarQueue, &temp, portMAX_DELAY);
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

  /* Was a Rx interrupt pending? */
  if (ulStatus & UART_INT_RX) {
    /* Read the character from the UART and send it to the queue. */
    // xQueueSendFromISR(xRxedChars, &HWREG(UART0_BASE + UART_O_DR), pdFALSE);
  }
}

/*-----------------------------------------------------------*/

void vGPIO_ISR(void) {
  portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

  /* Clear the interrupt. */
  // GPIOPinIntClear(GPIO_PORTC_BASE, mainPUSH_BUTTON);

  /* Wake the button handler task. */
  // xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
  //
  // portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}
