#include "freertosxx/event.h"

namespace freertosxx {
EventGroup::EventGroup() : handle_(xEventGroupCreate()) {}
EventGroup::~EventGroup() { vEventGroupDelete(handle_); }
}  // namespace freertosxx