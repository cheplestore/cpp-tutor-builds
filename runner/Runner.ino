// ============================================================
//  Runner.ino - wraps YOUR code (user_code.ino) with the
//  auto-updater. GitHub renames your setup()/loop() to
//  user_setup()/user_loop() before compiling.
//  You don't need to edit this file.
// ============================================================
#include "ota.h"

void setup() {
  otaStart();                    // updater runs in the background
  if (!otaSafeMode()) user_setup();
}

void loop() {
  if (otaSafeMode()) { delay(1000); return; }
  user_loop();
}
