#pragma once
#include "imgui.h"
#include <windows.h>

namespace myApp
{
    struct editor
    {
        int whiteBalance = 0;
        int exposure = 0;
        int contrast = 0;
        int highlights = 0;
        int shadows = 0;
        int whites = 0;
        int blacks = 0;
        int hue = 0;
        int saturation = 0;
        int redValue = 100;
        int greenValue = 100;
        int blueValue = 100;
        int blur = 0;
        int blurX = 0;
        int blurY = 0;
        int sharpness = 0;
        int vignette = 0;
    };
    void setupUI();
    void renderUI(bool& exit, float& scale, ImFont* head, ImFont* subhead, ImGuiIO& io);
}
