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
    time_t diff;
    static bool homePage = true;
    static char filePath[MAX_PATH];
    static char imagePath[MAX_PATH];
    static char imaegName[MAX_PATH];
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
    static bool updateInProcess = false;
    static bool updateInProgress = false;
    static int sliderWidth;
    static int numThreads;
    std::vector<std::thread> threads;
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

        //RGBA to HSVA
        for (unsigned h = 0; h < imageHeight; h++)
        {
            for (unsigned int w = 0; w < imageWidth; w++)
            {
                RGBA2HSVA(&imgRGB.at<cv::Vec4b>(h, w)[0]);
            }
        }

        //Contrast
        if (editor.contrast > 0)
        {
            for (unsigned int h = 0; h < imageHeight; h++)
            {
                for (unsigned int w = 0; w < imageWidth; w++)
                {
                    unsigned char* p = &imgRGB.at<cv::Vec4b>(h, w)[0];
                    unsigned short pixel = (100 - editor.contrast) * p[2] / 100.0f;
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
            }
        }
        else if (editor.contrast < 0)
        {
            for (unsigned int h = 0; h < imageHeight; h++)
            {
                for (unsigned int w = 0; w < imageWidth; w++)
                {
                    unsigned char* p = &imgRGB.at<cv::Vec4b>(h, w)[2];
                    *p = (*p - 127.5f) * (editor.contrast / 100.0f + 1) + 127.5f;
                }
            }
        }

        //Highlights, Whites, Shadows, Blacks
        if (editor.highlights || editor.whites || editor.shadows || editor.blacks)
        {
            for (unsigned int h = 0; h < imageHeight; h++)
            {
                for (unsigned int w = 0; w < imageWidth; w++)
                {
                    unsigned char* p = &imgRGB.at<cv::Vec4b>(h, w)[0];
                    short pixel = p[2];
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
            }
        }

        //Hue
        if (editor.hue)
        {
            unsigned char hueOffset = editor.hue * 2.55f;
            for (unsigned int h = 0; h < imageHeight; h++)
            {
                for (unsigned int w = 0; w < imageWidth; w++)
                {
                    unsigned char* p = &imgRGB.at<cv::Vec4b>(h, w)[0];
                    *p += hueOffset;
                }
            }
        }

        //Saturation
        if (editor.saturation - 50)
        {
            float multiplier = editor.saturation / 50.0f;
            if (editor.saturation > 50) multiplier *= multiplier;
            for (unsigned int h = 0; h < imageHeight; h++)
            {
                for (unsigned int w = 0; w < imageWidth; w++)
                {
                    unsigned char* p = &imgRGB.at<cv::Vec4b>(h, w)[1];
                    unsigned short value = *p * multiplier;
                    value = (value <= 255) * value + (value > 255) * 255;
                    *p = value;
                }
            }
        }


        //HSVA to RGBA
        for (unsigned int h = 0; h < imageHeight; h++)
        {
            for (unsigned int w = 0; w < imageWidth; w++)
            {
                HSVA2RGBA(&imgRGB.at<cv::Vec4b>(h, w)[0]);
            }
        }

        //Blur, X, Y
        int totalBlurX = editor.blur + editor.blurX;
        int totalBlurY = editor.blur + editor.blurY;
        if (totalBlurX || totalBlurY)
        {
            cv::GaussianBlur(imgRGB, imgRGB, cv::Size(totalBlurX * 2 + 1, totalBlurY * 2 + 1), 0, 0);
        }

        glDeleteTextures(1, &targetImageHandle);
        targetImageData = matToTexture(imgRGB);
        updateInProcess = false;
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

        // 'New file' icon
        img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\newFileButton.png", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        newFileIconData = matToTexture(imgRGB);
        img.release();
        newFileIconHandle = (GLuint)(intptr_t)newFileIconData;

        img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\newFileButtonHovered.png", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        newFileIconHoveredData = matToTexture(imgRGB);
        newFileIconHoveredHandle = (GLuint)(intptr_t)newFileIconHoveredData;

        //'Open file' icon
        img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\openFileButton.png", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        openFileIconData = matToTexture(imgRGB);
        img.release();
        openFileIconHandle = (GLuint)(intptr_t)openFileIconData;

        img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\openFileButtonHovered.png", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        openFileIconHoveredData = matToTexture(imgRGB);
        img.release();
        openFileIconHoveredHandle = (GLuint)(intptr_t)openFileIconHoveredData;

        //'Recover last session' icon
        img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\recoverButton.png", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        recoverIconData = matToTexture(imgRGB);
        img.release();
        recoverIconHandle = (GLuint)(intptr_t)newFileIconData;

        img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\recoverButtonHovered.png", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        recoverIconHoveredData = matToTexture(imgRGB);
        img.release();
        recoverIconHoveredHandle = (GLuint)(intptr_t)newFileIconHoveredData;

        //'Settings' icon
        img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\settingsButton.png", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        settingsIconData = matToTexture(imgRGB);
        img.release();
        settingsIconHandle = (GLuint)(intptr_t)newFileIconData;

        img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\settingsButtonHovered.png", -1);
        cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        settingsIconHoveredData = matToTexture(imgRGB);
        img.release();
        settingsIconHoveredHandle = (GLuint)(intptr_t)newFileIconHoveredData;

        //Work image
        img = cv::imread("C:\\Users\\parth\\OneDrive\\Pictures\\Screenshots\\Screenshot (88).jpg", -1);
        //img = cv::imread("C:\\Users\\parth\\Desktop\\Temporary\\subject.jpg", -1);
        //img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\settingsButtonHovered.png", -1);
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

        ImGui::SetCursorPos(ImVec2(40 SC, 240 SC));
        if (ImGui::ImageButton(newFileIconCurrent, ImVec2(410 SC, 103 SC), ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0.0588f, 0.0588f, 0.0588f, 1), ImVec4(1, 1, 1, 1)))
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
        if (ImGui::ImageButton(openFileIconCurrent, ImVec2(410 SC, 103 SC), ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0.0588f, 0.0588f, 0.0588f, 1), ImVec4(1, 1, 1, 1)))
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
        if (ImGui::ImageButton(recoverIconCurrent, ImVec2(410 SC, 103 SC), ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0.0588f, 0.0588f, 0.0588f, 1), ImVec4(1, 1, 1, 1)))
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
        if (ImGui::ImageButton(settingsIconCurrent, ImVec2(410 SC, 103 SC), ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0.0588f, 0.0588f, 0.0588f, 1), ImVec4(1, 1, 1, 1)))
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
        ImGui::Text("Update time: %f ms", difftime(end, start));
        // Update the image only when any change is made
        if (updatePending && !updateInProcess)
        {
            start = clock();
            updateImage();
            end = clock();
            updatePending = false;
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

        static ImDrawList* drawList = ImGui::GetWindowDrawList();
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
        ImGui::Text(" Zoom : %f", ImGui::GetWindowWidth() / imageWidth);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250 SC);
        ImGui::SliderFloat(" ", &zoomFactor, 5.0f, 1000.0f, "%.4f%%", ImGuiSliderFlags_Logarithmic);
        ImGui::SameLine();
        ImGui::Text("Position : (%dpx, %dpx)", posX, posY);
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(4,4));
        ImGui::SameLine();
        ImGui::Button("Export image");
        ImGui::EndChild();
        ImGui::End();

        ImGui::Begin("Sliders");

        sliderWidth = ImGui::GetWindowWidth() - 20 SC;
        ImGui::PushItemWidth(sliderWidth);
        ImGui::Text("White balance");
        updatePending = ImGui::SliderInt("##1", &editor.whiteBalance, -100.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Exposure");
        updatePending = ImGui::SliderInt("##2", &editor.exposure, -100.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Contrast");
        updatePending = ImGui::SliderInt("##3", &editor.contrast, -100.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Hightlights");
        updatePending = ImGui::SliderInt("##4", &editor.highlights, -100.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Shadows");
        updatePending = ImGui::SliderInt("##5", &editor.shadows, -100.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Whites");
        updatePending = ImGui::SliderInt("##6", &editor.whites, -100.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blacks");
        updatePending = ImGui::SliderInt("##7", &editor.blacks, -100.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Hue");
        updatePending = ImGui::SliderInt("##8", &editor.hue, 0.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Saturation");
        updatePending = ImGui::SliderInt("##9", &editor.saturation, 0.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Red value");
        updatePending = ImGui::SliderInt("##13", &editor.redValue, 0.0f, 200.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Green value");
        updatePending = ImGui::SliderInt("##16", &editor.greenValue, 0.0f, 200.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blue value");
        updatePending = ImGui::SliderInt("##19", &editor.blueValue, 0.0f, 200.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blur");
        updatePending = ImGui::SliderInt("##20", &editor.blur, 0.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blur X-axis");
        updatePending = ImGui::SliderInt("##21", &editor.blurX, 0.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Blur Y-axis");
        updatePending = ImGui::SliderInt("##22", &editor.blurY, 0.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1,10 SC));
        ImGui::Text("Sharpness");
        updatePending = ImGui::SliderInt("##23", &editor.sharpness, 0.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::BeginTooltip();
            ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
            ImGui::EndTooltip();
        }
        ImGui::Dummy(ImVec2(1, 10 SC));
        ImGui::Text("Vignette");
        updatePending = ImGui::SliderInt("##24", &editor.vignette, 0.0f, 100.0f, "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
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
                //img = cv::imread("C:\\Users\\parth\\Desktop\\photoEditor\\buttons\\settingsButtonHovered.png", -1);
                cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
                img = imgRGB.clone();
                imageWidth = img.cols;
                imageHeight = img.rows;
                originalImageData = matToTexture(imgRGB);
                originalImageHandle = (GLuint)(intptr_t)originalImageData;
                targetImageData = matToTexture(imgRGB);
                targetImageHandle = (GLuint)(intptr_t)targetImageData;
                updateImage();
            }
        }
        ImGui::End();
    }
    
    void renderUI(bool& exit, float& scale, ImFont* head, ImFont* subhead, ImGuiIO& io)
    {
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), 0);
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
