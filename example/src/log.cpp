#include "log.h"
#include <iostream>
#include <cstring>
#include <string.h>
#include <cstdarg>
#include <vector>

namespace logging {

const int MAX_CHARS = 256;

char kStdout[] = "stdout";
char msg_buffer[MAX_CHARS];

qbSystem system_out;
qbEvent std_out;

qbId program_id;

void initialize() {
  // Create a separate program/system to handle stdout.
  program_id = qb_create_program(kStdout);

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setprogram(attr, program_id);
    qb_systemattr_setcallback(attr,
        [](qbFrame* f) {
          std::cout << "[INFO] " << (char*)(f->event) << std::endl;
        });
    qb_system_create(&system_out, attr);
    qb_systemattr_destroy(&attr);
  }
  
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setprogram(attr, program_id);
    qb_eventattr_setmessagesize(attr, MAX_CHARS);
    qb_event_create(&std_out, attr);
    qb_event_subscribe(std_out, system_out);
    qb_eventattr_destroy(&attr);
  }

  qb_detach_program(program_id);
}

void out(const char* format, ...) {
  va_list args;
  va_start(args, format);

  char buf[256] = { 0 };

  vsprintf_s(buf, sizeof(buf), format, args);
  std::string s(buf);

  size_t num_msgs = 1 + (s.length() / MAX_CHARS);
  for (size_t i = 0; i < num_msgs; ++i) {
    std::string to_send = s.substr(i * MAX_CHARS, MAX_CHARS);
    memcpy(msg_buffer, to_send.c_str(), std::strlen(to_send.c_str()));
    qb_event_sendsync(std_out, msg_buffer);
    memset(msg_buffer, 0, MAX_CHARS);
  }

  va_end(args);
}

}  // namespace log
