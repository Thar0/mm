#ifndef PIKAPY_H
#define PIKAPY_H

#include "pikapy/pikascript-core/PikaObj.h"
#include "pikapy/pikascript-core/PikaVM.h"
PikaObj* New_PikaMain(Args* args);

// PikaObj* self = newRootObj("pikaMain", New_PikaMain)
// obj_newObj(self, (char*)name, (char*)name, func);
// pikaVM_run(self, script);
// obj_deinit(self);

#endif
