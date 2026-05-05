#pragma once

#include "../sdk/structs.h"

namespace cheat {
    struct Settings {
        bool espEnabled = true;
        bool aimbotEnabled = false;
        bool triggerbotEnabled = false;
        bool targetHead = true;
        float aimbotFOV = 5.0f;
        float aimbotSmooth = 4.0f;
    };

    extern Settings g_Settings;

    void RunFrame(int width, int height);
}