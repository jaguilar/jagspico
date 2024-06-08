#include "freertosxx/mutex.h"
#include "homeassistant/homeassistant.h"

using namespace homeassistant;

extern "C" void main_task(void* arg) {
  Cover c;
  c.set_id("test");
  printf("%s\n", c.Path().c_str());
  vTaskDelete(nullptr);
}