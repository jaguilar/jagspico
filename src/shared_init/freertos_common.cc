#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>

#include "pico/printf.h"
#include "task.h"

extern "C" {

void WaitForeverInCriticalSection() {
  portDISABLE_INTERRUPTS();
  taskENTER_CRITICAL();
  int setToZeroToContinue = 1;
  while (setToZeroToContinue != 0) {
    portNOP();
  }
  taskEXIT_CRITICAL();
  portENABLE_INTERRUPTS();
}

#if configUSE_MALLOC_FAILED_HOOK
void vApplicationMallocFailedHook(void) { WaitForeverInCriticalSection(); }
#endif

#if (configCHECK_FOR_STACK_OVERFLOW != 0)
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) {
  printf("stack overflow in %s\n", pcTaskName);
  WaitForeverInCriticalSection();
}
#endif
}