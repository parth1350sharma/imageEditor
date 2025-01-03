#include "application.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <shlobj.h>
#define SC * scale
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <gl/GL.h>
//#define GL_CLAMP_TO_EDGE 0x812F
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <thread>
#include <GLFW/glfw3.h>

namespace myApp
{
    time_t start;
    time_t end;
    static int updateTime = 0;
    static bool homePage = true;
    static char filePath[MAX_PATH];
    static char imagePath[MAX_PATH];
    static char imaegName[MAX_PATH];
    static char exportPath[MAX_PATH];
    static char pathFilter[300] = "PhotoEditor file(.phed)\0 * .phed\0";
    cv::Mat img;
    cv::Mat imgRGB;
    cv::Mat imgHSV;
    static unsigned imageWidth = 0;
    static unsigned imageHeight = 0;
    static int previewWidth;
    static int previewHeight;
    std::vector<cv::Mat> channels;
    static float zoomFactor = 100;
    static ImVec2 prevMousePos = {0,0};
    float prevMouseWheel;
    static int posX = 20;
    static int posY = 20;
    static float sizeX;
    static float sizeY;
    static bool imageDrag = false;
    static bool updatePending = false;
    static int updateStage = 0;
    std::vector<std::thread> threads;
    static bool exportWindow = false;
    static int sliderWidth;
    static int numThreads;
    static unsigned int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    static unsigned int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    char fpsTimeStamp = 0;
    GLuint newFileIconHandle;
    GLuint newFileIconHoveredHandle;
    ImTextureID newFileIconData;
    ImTextureID newFileIconHoveredData;
    ImTextureID newFileIconCurrent;
    GLuint openFileIconHandle;
    GLuint openFileIconHoveredHandle;
    ImTextureID openFileIconData;
    ImTextureID openFileIconHoveredData;
    ImTextureID openFileIconCurrent;
    GLuint recoverIconHandle;
    GLuint recoverIconHoveredHandle;
    ImTextureID recoverIconData;
    ImTextureID recoverIconHoveredData;
    ImTextureID recoverIconCurrent;
    GLuint settingsIconHandle;
    GLuint settingsIconHoveredHandle;
    ImTextureID settingsIconData;
    ImTextureID settingsIconHoveredData;
    ImTextureID settingsIconCurrent;

    GLuint originalImageHandle;
    ImTextureID originalImageData;
    GLuint targetImageHandle;
    ImTextureID targetImageData;

    struct editor editor;
    struct editor default;
    struct editor random = {10, -10, 20, -5, 5, -5, 5, 20, 20, 98, 97, 96, 0, 0, 0, 100, -50};

    static ImTextureID matToTexture(cv::Mat& image)
    {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.cols, image.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.ptr());
        return (ImTextureID)textureID;
    }
    static bool getFolderPath(char* path)
    {
        BROWSEINFO bi = { 0 };
        //bi.hwndOwner = NULL;
        bi.hwndOwner = GetDesktopWindow();
        bi.pidlRoot = NULL;
        bi.pszDisplayName = NULL;
        bi.lpszTitle = "Select an image file:";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        bi.lpfn = NULL;
        LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
        if (pidl != NULL)
        {
            SHGetPathFromIDListA(pidl, path);
            CoTaskMemFree(pidl);
            return true;
        }
        return false;
    }
    static bool getFilePath(char* filePath, char* filter)
    {
        OPENFILENAME ofn = { 0 };
        char szFile[MAX_PATH] = { 0 };

        ofn.lStructSize = sizeof(OPENFILENAME);
        //ofn.hwndOwner = GetDesktopWindow();
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileName(&ofn))
        {
            // Copy the selected file path
            strcpy(filePath, ofn.lpstrFile);
            return true;
        }

        return false;
    }
    static bool getSavePath(char* filePath, const char* filter)
    {
        OPENFILENAME ofn = { 0 };
        char szFile[MAX_PATH] = { 0 };

        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = NULL; // No owner window
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.lpstrDefExt = ""; // Default file extension if none provided
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

        if (GetSaveFileName(&ofn))
        {
            // Copy the selected file path
            strcpy(filePath, ofn.lpstrFile);
            return true;
        }

        return false;
    }

    static void RGBA2HSVA(unsigned char* p)
    {
        float fr = p[0] / 255.0f;
        float fg = p[1] / 255.0f;
        float fb = p[2] / 255.0f;
        float max = (p[0] * (p[0] >= p[1] && p[0] >= p[2]) + p[1] * (p[1] > p[0] && p[1] >= p[2]) + p[2] * (p[2] > p[0] && p[2] > p[1])) / 255.0f;
        if (max == 0) return;
        float min = (p[0] * (p[0] <= p[1] && p[0] <= p[2]) + p[1] * (p[1] < p[0] && p[1] <= p[2]) + p[2] * (p[2] < p[0] && p[2] < p[1])) / 255.0f;
        float delta = max - min;

        float fh = 0, fs = delta / max, fv = max;

        if (delta != 0) {
            if (max == fr) {
                fh = (fg - fb) / delta + (fg < fb ? 6 : 0);
            }
            else if (max == fg) {
                fh = (fb - fr) / delta + 2;
            }
            else if (max == fb) {
                fh = (fr - fg) / delta + 4;
            }
            fh /= 6;
        }

        p[0] = (unsigned char)(fh * 255);
        p[1] = (unsigned char)(fs * 255);
        p[2] = (unsigned char)(fv * 255);
    }
    static void HSVA2RGBA(unsigned char* p)
    {
        float fv = p[2] / 255.0f; // Value (0-1 range)
        int i = (int)(p[0] * 0.0235294f);
        float f = p[0] * 0.0235294f - i;
        float p_value = fv * (1.0f - p[1] / 255.0f);
        float q = fv * (1.0f - p[1] * f / 255);
        float t = fv * (1.0f - p[1] * (1.0f - f) / 255);

        float fr, fg, fb;
        switch (i % 6)
        {
        case 0: fr = fv; fg = t; fb = p_value; break;
        case 1: fr = q; fg = fv; fb = p_value; break;
        case 2: fr = p_value; fg = fv; fb = t; break;
        case 3: fr = p_value; fg = q; fb = fv; break;
        case 4: fr = t; fg = p_value; fb = fv; break;
        case 5: fr = fv; fg = p_value; fb = q; break;
        }

        p[0] = (unsigned char)(fr * 255);
        p[1] = (unsigned char)(fg * 255);
        p[2] = (unsigned char)(fb * 255);
    }
    static void updateImage_thread(unsigned low, unsigned high)
    {
        unsigned maxDistance = sqrt(imageHeight * imageHeight + imageWidth * imageWidth) / 2;
        for (unsigned h = low; h < high; h++)
        {
            for (unsigned int w = 0; w < imageWidth; w++)
            {
                unsigned char* p = &imgRGB.at<cv::Vec4b>(h, w)[0];
                short pixel;
                RGBA2HSVA(p);
                if (editor.contrast > 0)
                {
                    pixel = (100 - editor.contrast) * p[2] / 100.0f;
                    if (pixel > 127.5f - p[2])
                    {
                        pixel = (100 - editor.contrast) * (p[2] - 255) / 100.0f + 255;
                        if (pixel < 382.5f - p[2])
                        {
                            pixel = 100 * (p[2] - 127.5f) / (100 - editor.contrast) + 127.5f;
                        }
                    }
                    pixel = p[2] * (float)pixel / p[2];
                    p[2] = pixel * (pixel <= 255) + 255 * (pixel > 255);
                }
                else if (editor.contrast < 0)
                {
                    p[2] = (p[2] - 127.5f) * (editor.contrast / 100.0f + 1) + 127.5f;
                }
                if (editor.highlights || editor.whites || editor.shadows || editor.blacks)
                {
                    pixel = p[2];
                    if (p[2] > 200)
                    {
                        //Highlights
                        if (editor.highlights)
                        {
                            pixel = p[2] + (p[2] - 200) * (editor.highlights) / 20.0f;
                            pixel = pixel * (pixel > 0);
                            pixel = pixel * (pixel <= 255) + 255 * (pixel > 255);
                        }
                        //Whites
                        if (editor.whites && p[1] < 40)
                        {
                            pixel = pixel + (p[2] - 200) * (editor.whites) / 20.0f;
                            pixel = pixel * (pixel > 0);
                            pixel = pixel * (pixel <= 255) + 255 * (pixel > 255);
                        }
                    }
                    else if (p[2] < 128)
                    {
                        //Shadows
                        if (editor.shadows)
                        {
                            pixel = p[2] + (127 - p[2]) * (editor.shadows) / 40.0f;
                            pixel = pixel * (pixel > 0);
                            pixel = pixel * (pixel <= 255) + 255 * (pixel > 255);
                        }
                        //Blacks
                        if (editor.blacks && p[2] < 60)
                        {
                            pixel = pixel + (60 - p[2]) * (editor.blacks) / 40.0f;
                            pixel = pixel * (pixel > 0);
                            pixel = pixel * (pixel <= 255) + 255 * (pixel > 255);
                        }
                    }

                    p[2] = pixel;
                }
                if (editor.hue)
                {
                    unsigned char hueOffset = editor.hue * 2.55f;
                    *p += hueOffset;
                }
                if (editor.saturation)
                {
                    float multiplier = (editor.saturation + 100) / 100.0f;
                    if (editor.saturation > 0) multiplier *= multiplier;
                    unsigned short value = p[1] * multiplier;
                    value = (value <= 255) * value + (value > 255) * 255;
                    p[1] = value;
                }
                if (editor.vignette)
                {
                    unsigned threshold = maxDistance / 2;
                    unsigned distance = sqrt((h - imageHeight / 2) * (h - imageHeight / 2) + (w - imageWidth / 2) * (w - imageWidth / 2));

                    if (distance > threshold)
                    {
                        //Adjust Value
                        pixel = p[2] + editor.vignette * ((float)(distance - threshold) / maxDistance) * 10;
                        pixel *= (pixel > 0);
                        p[2] = (pixel <= 255) * pixel + (pixel > 255) * 255;

                        //Adjust saturation
                        if (editor.vignette > 0)
                        {
                            pixel = p[1] - editor.vignette * ((float)(distance - threshold) / maxDistance) * 10;
                            pixel *= (pixel > 0);
                            p[1] = (pixel <= 255) * pixel + (pixel > 255) * 255;
                        }
                    }
                }
                HSVA2RGBA(&imgRGB.at<cv::Vec4b>(h, w)[0]);
            }
        }
    }
    static void updateImage()
    {
        float totalRedValue = 100.0f;
        float totalGreenValue = 100.0f;
        float totalBlueValue = 100.0f;
        //img is original image, imgRGB is edited image
        imgRGB = img.clone();

        split(imgRGB, channels); // Channels[0] is red, [1] is green, [2] is blue

        //White balance
        //Amber(Hot temperature): rgb(255, 191, 0)
        //So cold temperature is: rgb(0, 64, 255)
        if (editor.whiteBalance > 0)
        {
            channels[0] = ((100 + editor.whiteBalance) / 100.0f) * channels[0];
            channels[1] = (-editor.whiteBalance / 100.0f) * 64 + ((100 + editor.whiteBalance) / 100.0f) * channels[1];
            channels[2] = (-editor.whiteBalance / 100.0f) * 255 + ((100 + editor.whiteBalance) / 100.0f) * channels[2];
        }
        else if (editor.whiteBalance < 0)
        {
            channels[0] = ((100 + editor.whiteBalance) / 100.0f) * channels[0];
            channels[1] = (editor.whiteBalance / 100.0f) * 191 + ((100 - editor.whiteBalance) / 100.0f) * channels[1];
            channels[2] = ((100 - editor.whiteBalance) / 100.0f) * channels[2];
        }

        
        //Value, exposure
        totalRedValue = editor.redValue + editor.exposure;
        channels[0] *= totalRedValue / 100.0f;
        totalGreenValue = editor.greenValue + editor.exposure;
        channels[1] *= totalGreenValue / 100.0f;
        totalBlueValue = editor.blueValue + editor.exposure;
        channels[2] *= totalBlueValue / 100.0f;
        
        merge(channels, imgRGB);

        std::vector<std::thread> threads;
        for (int n = 0; n < numThreads; n++)
        {
            threads.emplace_back(updateImage_thread, n * imageHeight / numThreads, (n + 1) * imageHeight / numThreads);
        }
        for (int n = 0; n < numThreads; n++)
        {
            threads[n].join();
        }

        //Blur, X, Y
        int totalBlurX = editor.blur + editor.blurX;
        int totalBlurY = editor.blur + editor.blurY;
        if (totalBlurX || totalBlurY)
        {
            cv::GaussianBlur(imgRGB, imgRGB, cv::Size(totalBlurX * 2 + 1, totalBlurY * 2 + 1), 0, 0);
        }

        //Sharpness
        if (editor.sharpness)
        {
            float sharpness = (editor.sharpness / (1.0f * (editor.sharpness > 0) + 4.0f * (editor.sharpness < 0)) + 1) / 4;
            float sharpnessInverse = (1 - sharpness) / 8;
            //The sum of all elements of the kernal should be 1
            cv::Mat sharpening_filter = (cv::Mat_<float>(3, 3) << sharpnessInverse, sharpnessInverse, sharpnessInverse,
                                                                  sharpnessInverse, sharpness, sharpnessInverse,
                                                                  sharpnessInverse, sharpnessInverse, sharpnessInverse);
            cv::filter2D(imgRGB, imgRGB, -1, sharpening_filter);
        }

        //glDeleteTextures(1, &targetImageHandle); // This line has been added outside the function instead because of unknown bug because of threading
        //targetImageData = matToTexture(imgRGB); 
        updateStage = 2;
    }

    void setupUI()
    {
        numThreads = std::thread::hardware_concurrency();
        if (!numThreads)
        {
            numThreads = 1;
        }
        else if (numThreads > 3)
        {
            numThreads--;
        }

        cv::Mat icon;
        std::string iconPath;

        // 'New file' icon
        iconPath = R"(..\..\..\buttons\newFileButton.png)";
        icon = cv::imread(iconPath, -1);
        cv::cvtColor(icon, icon, cv::COLOR_BGRA2RGBA);
        newFileIconData = matToTexture(icon);
        icon.release();
        newFileIconHandle = (GLuint)(intptr_t)newFileIconData;

        iconPath = R"(..\..\..\buttons\newFileButtonHovered.png)";
        icon = cv::imread(iconPath, -1);
        cv::cvtColor(icon, icon, cv::COLOR_BGRA2RGBA);
        newFileIconHoveredData = matToTexture(icon);
        newFileIconHoveredHandle = (GLuint)(intptr_t)newFileIconHoveredData;

        //'Open file' icon
        iconPath = R"(..\..\..\buttons\openFileButton.png)";
        icon = cv::imread(iconPath, -1);
        cv::cvtColor(icon, icon, cv::COLOR_BGRA2RGBA);
        openFileIconData = matToTexture(icon);
        icon.release();
        openFileIconHandle = (GLuint)(intptr_t)openFileIconData;

        iconPath = R"(..\..\..\buttons\openFileButtonHovered.png)";
        icon = cv::imread(iconPath, -1);
        cv::cvtColor(icon, icon, cv::COLOR_BGRA2RGBA);
        openFileIconHoveredData = matToTexture(icon);
        icon.release();
        openFileIconHoveredHandle = (GLuint)(intptr_t)openFileIconHoveredData;

        //'Recover last session' icon
        iconPath = R"(..\..\..\buttons\recoverButton.png)";
        icon = cv::imread(iconPath, -1);
        cv::cvtColor(icon, icon, cv::COLOR_BGRA2RGBA);
        recoverIconData = matToTexture(icon);
        icon.release();
        recoverIconHandle = (GLuint)(intptr_t)newFileIconData;

        iconPath = R"(..\..\..\buttons\recoverButtonHovered.png)";
        icon = cv::imread(iconPath, -1);
        cv::cvtColor(icon, icon, cv::COLOR_BGRA2RGBA);
        recoverIconHoveredData = matToTexture(icon);
        icon.release();
        recoverIconHoveredHandle = (GLuint)(intptr_t)newFileIconHoveredData;

        //'Settings' icon
        iconPath = R"(..\..\..\buttons\settingsButton.png)";
        icon = cv::imread(iconPath, -1);
        cv::cvtColor(icon, icon, cv::COLOR_BGRA2RGBA);
        settingsIconData = matToTexture(icon);
        icon.release();
        settingsIconHandle = (GLuint)(intptr_t)newFileIconData;

        iconPath = R"(..\..\..\buttons\settingsButtonHovered.png)";
        icon = cv::imread(iconPath, -1);
        cv::cvtColor(icon, icon, cv::COLOR_BGRA2RGBA);
        settingsIconHoveredData = matToTexture(icon);
        icon.release();
        settingsIconHoveredHandle = (GLuint)(intptr_t)newFileIconHoveredData;

        //Work image
        img = cv::imread(R"(..\..\..\sample image.jpg)", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        img = imgRGB.clone();
        imageWidth = img.cols;
        imageHeight = img.rows;
        originalImageData = matToTexture(imgRGB);
        originalImageHandle = (GLuint)(intptr_t)originalImageData;
        targetImageData = matToTexture(imgRGB);
        targetImageHandle = (GLuint)(intptr_t)targetImageData;
    }
    static void displayHomePage(bool& exit, float& scale, ImFont* head, ImFont* subhead)
    {
        ImGui::Begin("Home Page", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::PushFont(head);
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() / 2 - 400 SC, 60 SC));
        ImGui::Text("Welcome to PhotoEditor");
        ImGui::PopFont();

        //Remove borders from icons
        ImGuiStyle& style = ImGui::GetStyle();
        float originalFrameBorderSize = style.FrameBorderSize;
        ImVec2 originalFramePadding = style.FramePadding;
        style.FrameBorderSize = 0.0f;
        style.FramePadding = ImVec2(0.0f, 0.0f);
        ImGui::SetCursorPos(ImVec2(40 SC, 240 SC));
        if (ImGui::ImageButton("a", newFileIconCurrent, ImVec2(410 SC, 103 SC), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.0588f, 0.0588f, 0.0588f, 1)))
        {
            //openMainWindow();
        }
        if (ImGui::IsItemHovered())
        {
            newFileIconCurrent = newFileIconHoveredData;
            ImGui::BeginTooltip();
            ImGui::Text("Create a new .phed file");
            ImGui::EndTooltip();
        }
        else
        {
            newFileIconCurrent = newFileIconData;
        }

        ImGui::SetCursorPos(ImVec2(40 SC, 345 SC));
        if (ImGui::ImageButton("b", openFileIconCurrent, ImVec2(410 SC, 103 SC), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.0588f, 0.0588f, 0.0588f, 1)))
        {
            homePage = !getFilePath(filePath, "PhotoEditor file(.phed)\0 * .phed\0");
        }
        if (ImGui::IsItemHovered())
        {
            openFileIconCurrent = openFileIconHoveredData;
            ImGui::BeginTooltip();
            ImGui::Text("Open an existing .phed file\nfrom file explorer");
            ImGui::EndTooltip();
        }
        else
        {
            openFileIconCurrent = openFileIconData;
        }

        ImGui::SetCursorPos(ImVec2(40 SC, 450 SC));
        if (ImGui::ImageButton("c", recoverIconCurrent, ImVec2(410 SC, 103 SC), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.0588f, 0.0588f, 0.0588f, 1)))
        {
            homePage = false;
            //recoverLastSession();
        }
        if (ImGui::IsItemHovered())
        {
            recoverIconCurrent = recoverIconHoveredData;
            ImGui::BeginTooltip();
            ImGui::Text("Recover the last session.\nCan be useful when the last\nfile is not saved for some reason");
            ImGui::EndTooltip();
        }
        else
        {
            recoverIconCurrent = recoverIconData;
        }

        ImGui::SetCursorPos(ImVec2(40 SC, 555 SC));
        if (ImGui::ImageButton("d", settingsIconCurrent, ImVec2(410 SC, 103 SC), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.0588f, 0.0588f, 0.0588f, 1)))
        {
            //openSettings();
        }
        if (ImGui::IsItemHovered())
        {
            settingsIconCurrent = settingsIconHoveredData;
            ImGui::BeginTooltip();
            ImGui::Text("Show a window consisting of \nall the options and preferences");
            ImGui::EndTooltip();
        }
        else
        {
            settingsIconCurrent = settingsIconData;
        }
        // Restore original settings
        style.FrameBorderSize = originalFrameBorderSize;
        style.FramePadding = originalFramePadding;


        // Create table of recent files
        ImGui::PushFont(subhead);
        ImGui::SetCursorPos(ImVec2(520 SC, 180 SC));
        ImGui::Text("Recent files");
        ImGui::PopFont();
        ImGui::SetCursorPosX(520 SC);
        if (ImGui::BeginTable("Recent files", 4, ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, ImVec2(800.0f SC, 500.0f SC)))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_None);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_None);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None);
            ImGui::TableHeadersRow();

            // Demonstrate using clipper for large vertical lists
            ImGuiListClipper clipper;
            clipper.Begin(50);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                {
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, 100.0f SC);
                    for (int column = 0; column < 4; column++)
                    {
                        ImGui::TableSetColumnIndex(column);
                        ImGui::Text("\n\nHello %d,%d", row, column);
                    }
                }
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
    static void displayMainWindow(bool& exit, float& scale, ImFont* head, ImFont* subhead, ImGuiIO& io)
    {
        ImGui::Begin("Main Window", NULL, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
        if(updateStage != 0 && updateTime > 200)
            ImGui::Text("Update time: %d ms (Processing...)", updateTime);
        else
            ImGui::Text("Update time: %d ms", updateTime);

        if (ImGui::Button("Random sliders"))
        {
            editor = random;
            updatePending = true;
        }

        // Update the image only when any change is made
        if (updatePending && updateStage == 0)
        {
            start = clock();
            //updateImage();
            std::thread worker(updateImage);
            updateStage = 1;
            worker.detach();

            updatePending = false;
        }
        if (updateStage == 2)
        {
            glDeleteTextures(1, &targetImageHandle);
            targetImageData = matToTexture(imgRGB);
            updateStage = 0;
            end = clock();
            updateTime = difftime(end, start);
        }

        ImGui::SetCursorPos(ImVec2(posX, posY));
        sizeX = imageWidth SC * zoomFactor / 100;
        sizeY = imageHeight SC * zoomFactor / 100;
        ImGui::Image(targetImageData, ImVec2(sizeX, sizeY), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0.5f, 0.5f, 0.5f, 1));
        if (ImGui::IsWindowHovered())
        {
            zoomFactor += io.MouseWheel * 5;
            if (zoomFactor < 2)
            {
                zoomFactor = 2;
            }
            else if (zoomFactor > 1000)
            {
                zoomFactor = 1000;
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                posX -= prevMousePos.x - io.MousePos.x;
                posY -= prevMousePos.y - io.MousePos.y;
            }
        }
        prevMousePos = io.MousePos;

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 49 SC);
        ImGui::BeginChild("Stastistics", ImVec2(0, 0), true);
        if (ImGui::Button("Reset view"))
        {
            posX = 20;
            posY = 20;
            //zoomFactor = 100;
            zoomFactor = ImGui::GetWindowWidth() / imageWidth * 100;
        }
        ImGui::SameLine();
        ImGui::Text(" Zoom : ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250 SC);
        ImGui::SliderFloat(" ", &zoomFactor, 5.0f, 1000.0f, "%.4f%%", ImGuiSliderFlags_Logarithmic);
        ImGui::SameLine();
        ImGui::Text("Position : (%dpx, %dpx)", posX, posY);
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(4,4));
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 118 SC);
        if (ImGui::Button("Export image"))
        {
            exportWindow = true;
        }
        if (exportWindow)
        {
            static int exportExtension = 0;
            static char exportFilter[32];
            strcpy(exportFilter, "JPG image\0*.jpg;\0");
            ImGui::Begin("Export window", &exportWindow, ImGuiWindowFlags_NoDocking);
            ImGui::Text("File extension: ");
            ImGui::RadioButton(".JPG/.JPEG", &exportExtension, 0);
            ImGui::RadioButton(".PNG", &exportExtension, 1);
            ImGui::RadioButton(".TIF/.TIFF", &exportExtension, 2);
            switch (exportExtension)
            {
            case 0:
                strcpy(exportFilter, "JPG image\0*.jpg;\0");
                break;
            case 1:
                strcpy(exportFilter, "PNG image\0*.png;\0");
                break;
            case 2:
                strcpy(exportFilter, "TIFF image\0*.tif;\0");
                break;
            }
            ImGui::Text("File path:");
            ImGui::InputText("##1", exportPath, MAX_PATH);
            ImGui::SameLine(0.0f, 2.0f SC);
            if (ImGui::Button("Browse"))
            {
                getSavePath(exportPath, exportFilter);
            }
            if (ImGui::Button("Export"))
            {
                cv::Mat imgExport;
                cv::cvtColor(imgRGB, imgExport, cv::COLOR_RGBA2BGRA);
                cv::imwrite(exportPath, imgExport);
                exportWindow = false;
            }
            ImGui::End();
        }
        ImGui::EndChild();
        ImGui::End();

        ImGui::Begin("Sliders");

        if (ImGui::Button("Reset to default"))
        {
            editor = default;
            updatePending = true;
        }
        ImGui::Dummy(ImVec2(1, 10 SC));

        sliderWidth = ImGui::GetWindowWidth() - 20 SC;
        ImGui::PushItemWidth(sliderWidth);
        ImGui::Text("White balance");
        updatePending = ImGui::SliderInt("##1", &editor.whiteBalance, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Exposure");
        updatePending = ImGui::SliderInt("##2", &editor.exposure, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Contrast");
        updatePending = ImGui::SliderInt("##3", &editor.contrast, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Hightlights");
        updatePending = ImGui::SliderInt("##4", &editor.highlights, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Shadows");
        updatePending = ImGui::SliderInt("##5", &editor.shadows, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Whites");
        updatePending = ImGui::SliderInt("##6", &editor.whites, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blacks");
        updatePending = ImGui::SliderInt("##7", &editor.blacks, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Hue");
        updatePending = ImGui::SliderInt("##8", &editor.hue, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Saturation");
        updatePending = ImGui::SliderInt("##9", &editor.saturation, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Red value");
        updatePending = ImGui::SliderInt("##13", &editor.redValue, 0, 200, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Green value");
        updatePending = ImGui::SliderInt("##16", &editor.greenValue, 0, 200, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blue value");
        updatePending = ImGui::SliderInt("##19", &editor.blueValue, 0, 200, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blur");
        updatePending = ImGui::SliderInt("##20", &editor.blur, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blur X-axis");
        updatePending = ImGui::SliderInt("##21", &editor.blurX, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blur Y-axis");
        updatePending = ImGui::SliderInt("##22", &editor.blurY, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Sharpness");
        updatePending = ImGui::SliderInt("##23", &editor.sharpness, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1, 10 SC));
        ImGui::Text("Vignette");
        updatePending = ImGui::SliderInt("##24", &editor.vignette, -100, 100, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::PopItemWidth();

        ImGui::End();

        ImGui::Begin("Image data");
        ImGui::Text("Name: \nLocation: \nResolution: \nSize: \nBit depth: \nPreview: \n");
        previewWidth = ImGui::GetWindowWidth() - 16 SC;
        if (previewWidth > 200 SC)
            previewWidth = 200 SC;
        previewHeight = (float)previewWidth * imageHeight / imageWidth;
        ImGui::Image(originalImageData, ImVec2(previewWidth, previewHeight), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0.5f, 0.5f, 0.5f, 1));
        if (ImGui::Button("Replace image"))
        {
            if (getFilePath(imagePath, "PNG image\0*.png;\0JPG image\0*.jpg;\0TIF image\0*.tif;*.tiff\0"))
            {
                img = cv::imread(imagePath, -1);
                cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
                img = imgRGB.clone();
                imageWidth = img.cols;
                imageHeight = img.rows;
                if (numThreads > imageHeight) numThreads = imageHeight;
                originalImageData = matToTexture(img);
                originalImageHandle = (GLuint)(intptr_t)originalImageData;
                targetImageData = matToTexture(imgRGB);
                targetImageHandle = (GLuint)(intptr_t)targetImageData;
                updatePending = true;
            }
        }
        ImGui::End();
    }
    
    void renderUI(bool& exit, float& scale, ImFont* head, ImFont* subhead, ImGuiIO& io)
    {
        ImGui::DockSpaceOverViewport(ImGui::GetWindowDockID(), 0);
        // Vsync
        //Reduce fps when mouse not moving
        if (prevMousePos.x == io.MousePos.x && prevMousePos.y == io.MousePos.y && prevMouseWheel == io.MouseWheel)
        {
            if (fpsTimeStamp == 0)
                glfwSwapInterval(8);
        }
        else
        {
            fpsTimeStamp = 50;
            glfwSwapInterval(1);
        }
        fpsTimeStamp -= (fpsTimeStamp > 0);
        prevMouseWheel = io.MouseWheel;
        if (homePage)
        {
            displayHomePage(exit, scale, head, subhead);
        }
        else
        {
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    ImGui::MenuItem("New", "Ctrl+N");
                    if (ImGui::MenuItem("Open", "Ctrl+O"))
                    {
                        getFilePath(filePath, "PhotoEditor file(.phed)\0 * .phed\0");
                    }
                    ImGui::MenuItem("Save", "Ctrl+S");
                    ImGui::MenuItem("Save As", "Ctrl+Shift+S");
                    if (ImGui::BeginMenu("More.."))
                    {
                        ImGui::MenuItem("Hello");
                        ImGui::MenuItem("Sailor");
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit"))
                {
                    ImGui::MenuItem("Undo", "Ctrl+Z");
                    ImGui::MenuItem("Redo", "Ctrl+Y");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Window"))
                {
                    ImGui::MenuItem("Reset layout");
                    ImGui::MenuItem("Save current layout");
                    if (ImGui::BeginMenu("Use saved layout"))
                    {
                        ImGui::MenuItem("Layout 1");
                        ImGui::MenuItem("Layout 2");
                        ImGui::MenuItem("Layout 3");
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
            displayMainWindow(exit, scale, head, subhead, io);
            
        }

    }

}
