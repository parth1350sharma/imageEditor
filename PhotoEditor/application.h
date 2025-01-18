#pragma once
#include "imgui.h"
#include <windows.h>

namespace myApp
{
    struct Editor
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

    struct OutputParams
    {
        int format = 1; //1 is JPG, 2 is PNG, 3 is WEBP, 4 is TIF
        int jpgQuality = 95; //Higher is better quality
        int pngCompression = 3; //Lower is better quality
        int webpQuality = 75; //Higher is better quality
        int tifCompression = 1; //Different number has different compression types
        char path[MAX_PATH];
    };

    struct ImageMetadata
    {
        char name[MAX_PATH] = "Default value";
        char folder[MAX_PATH] = "Default value";
        int width = 0;
        int height = 0;
        unsigned int sizeOnDisk = 0;
    };

    void setupUI();
    void renderUI(bool& exit, float& scale, ImFont* head, ImFont* subhead, ImGuiIO& io);
}
