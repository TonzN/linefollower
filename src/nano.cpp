// Eldre oppsett: Uno kommuniserer med Nano over I2C.
// Den nye hoved-Nanoen bruker master.cpp som inngangspunkt.
#include <Arduino.h>
#include "control_runtime.h"

void setup()
{
    control_runtime::begin();
}

void loop()
{
    control_runtime::tick();
}
