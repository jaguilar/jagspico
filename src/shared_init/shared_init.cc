#include "shared_init.h"

#include <FreeRTOS.h>

#include "FreeRTOSConfig.h"
#include "pico/platform.h"
#ifdef RASPBERRYPI_PICO_W
#include "cyw43_country.h"
#include "cyw43_ll.h"
#include "pico/cyw43_arch.h"
#endif
#include "pico/multicore.h"
#include "pico/printf.h"
#include "pico/stdlib.h"
#include "portmacro.h"
#include "projdefs.h"
#include "task.h"

#if LWIP_MDNS_RESPONDER
#include "lwip/apps/mdns.h"
#endif
#if LWIP_DEBUG
#include "lwip/debug.h"
#endif

extern "C" {

#if configSUPPORT_STATIC_ALLOCATION == 1
// Copied from the FreeRTOS docs.
void vApplicationGetIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer,
    configSTACK_DEPTH_TYPE *puxIdleTaskStackSize) {
  static StaticTask_t xIdleTaskTCB;
  static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;
  *puxIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer,
    configSTACK_DEPTH_TYPE *puxTimerTaskStackSize) {
  static StaticTask_t xTimerTaskTCB;
  static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *puxTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationGetPassiveIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer,
    configSTACK_DEPTH_TYPE *puxIdleTaskStackSize,
    BaseType_t xPassiveIdleTaskIndex) {
  static StaticTask_t xIdleTaskTCBs[configNUMBER_OF_CORES - 1];
  static StackType_t uxIdleTaskStacks[configNUMBER_OF_CORES - 1]
                                     [configMINIMAL_STACK_SIZE];
  *ppxIdleTaskTCBBuffer = &(xIdleTaskTCBs[xPassiveIdleTaskIndex]);
  *ppxIdleTaskStackBuffer = &(uxIdleTaskStacks[xPassiveIdleTaskIndex][0]);
  *puxIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
#endif

static TaskHandle_t g_init_task;

#if LWIP_MDNS_RESPONDER
static void srv_txt(struct mdns_service *service, void *txt_userdata) {
  err_t res;
  LWIP_UNUSED_ARG(txt_userdata);

  res = mdns_resp_add_service_txtitem(service, "path=/", 6);
  LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
}

static void mdns_example_report(
    struct netif *netif, u8_t result, s8_t service) {
  LWIP_PLATFORM_DIAG(
      ("mdns status[netif %d][service %d]: %d\n", netif->num, service, result));
}
#endif

static void init_task(void *arg) {
  printf("will initialize wifi\n");

#ifdef RASPBERRYPI_PICO_W
  if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
    panic("Wi-Fi init failed\n");
  }
  printf("wifi init done\n");
#if CYW43_LWIP
  cyw43_arch_enable_sta_mode();
  printf("will connect wifi\n");
  {
    int error;
    if ((error = cyw43_arch_wifi_connect_blocking(
             WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK)) != 0) {
      printf(
          "Failed to connect to %s (pwd %s): %d\n",
          WIFI_SSID,
          WIFI_PASSWORD,
          error);
    } else {
      printf("wifi connected\n");
    }
  }

  for (int i = 0; i < 3; ++i) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
#endif
#if LWIP_MDNS_RESPONDER
  mdns_resp_register_name_result_cb(mdns_example_report);
  mdns_resp_init();
  mdns_resp_add_netif(netif_default, CYW43_HOST_NAME);
  mdns_resp_add_service(
      netif_default, "telnet", "_telnet", DNSSD_PROTO_TCP, 23, srv_txt, NULL);
  mdns_resp_announce(netif_default);
#endif  // LWIP_MDNS_RESPONDER
#endif  // RASPBERRYPI_PICO_W
  main_task(arg);
}

int main(void) {
  stdio_init_all();

  BaseType_t err =
      xTaskCreate(init_task, "__init_task", 1024, NULL, 1, &g_init_task);
  configASSERT(err == pdPASS);
  vTaskStartScheduler();
}
}