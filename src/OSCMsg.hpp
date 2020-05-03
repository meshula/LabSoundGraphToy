
#pragma once

#include "queue_spsc.hpp"
#include <string>

struct OSCMsg
{
    const char * addr = nullptr;
    int addr_id = 0;
    int argc = 0;
    float data[4] = { 0,0,0,0 };
};
