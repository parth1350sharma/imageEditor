#include "application.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <shlobj.h>
#include "stb_image.h"
#include <gl/GL.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <thread>
#include <GLFW/glfw3.h>
#include <sys/stat.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>

//#pragma comment(lib, "Shlwapi.lib") // Link Shlwapi.lib for PathFileExists

#define SC * scale
#define STB_IMAGE_IMPLEMENTATION
//#define GL_CLAMP_TO_EDGE 0x812F

namespace myApp
{

    #pragma region sliders
    static const int sliderStringCount = 17;
    static const int jumpLength = 14;
    static const char* sliderStrings = "White balance\0"
        "Exposure\0\0\0\0\0\0"
        "Contrast\0\0\0\0\0\0"
        "Highlights\0\0\0\0"
        "Shadows\0\0\0\0\0\0\0"
        "Whites\0\0\0\0\0\0\0\0"
        "Blacks\0\0\0\0\0\0\0\0"
        "Hue\0\0\0\0\0\0\0\0\0\0\0"
        "Saturation\0\0\0\0"
        "Red value\0\0\0\0\0"
        "Green value\0\0\0"
        "Blue value\0\0\0\0"
        "Blur\0\0\0\0\0\0\0\0\0\0"
        "Blur X-axis\0\0\0"
        "Blur Y-axis\0\0\0"
        "Sharpness\0\0\0\0\0"
        "Vignette\0";
    static const int sliderMin[17] = { -100, -100, -100, -100, -100, -100, -100, 0, -100, 0, 0, 0, 0, 0, 0, -100, -100 };
    static const int sliderMax[17] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 200, 200, 200, 100, 100, 100, 100, 100 };
    #pragma endregion

    #pragma region UI Handling Variables
    static unsigned int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    static unsigned int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    char fpsTimeStamp = 0;
    static int previewWidth;
    static int previewHeight;
    static float zoomFactor = 100;
    static ImVec2 prevMousePos = { 0,0 };
    float prevMouseWheel;
    static int posX = 20;
    static int posY = 20;
    static float sizeX;
    static float sizeY;
    static int sliderWidth;
    #pragma endregion

    #pragma region Thread handelling variables
    time_t start;
    time_t end;
    static int updateTime = 0;
    static int numThreads;
    static bool updatePending = false;
    static int updateStage = 0;
    #pragma endregion

    #pragma region Icon handelling variables
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
    #pragma endregion

    #pragma region Image handelling variables
    cv::Mat img;
    cv::Mat imgRGB;
    cv::Mat imgHSV;
    static unsigned imageWidth = 0;
    static unsigned imageHeight = 0;
    GLuint originalImageHandle;
    ImTextureID originalImageData;
    GLuint targetImageHandle;
    ImTextureID targetImageData;
    #pragma endregion

    #pragma region File handelling variables
    static char phedFilePath[MAX_PATH] = "\0";
    static char phedFileContent[MAX_PATH + 68 + 1] = "\0";
    static char imagePath[MAX_PATH] = "\0";
    static bool savePending = false;
    static bool prevMouseDown = false;
    #pragma endregion

    #pragma region Other variables
    static bool homePage = true;
    static bool exportWindow = false;
    static bool savePendingWindow = false;
    static bool settingsWindow = false;
    static int exportSuccessTimer = 0;
    struct Editor current;
    struct Editor previous;
    struct Editor default;
    struct Editor random = { 10, -10, 20, -5, 5, -5, 5, 20, 20, 98, 97, 96, 0, 0, 0, 100, -50 };
    struct OutputParams output;
    struct ImageMetadata metadata;
    std::vector<int> exportParameters = { cv::IMWRITE_JPEG_QUALITY, 90 };

    #pragma endregion

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
    static bool isEditorEqual(Editor& a, Editor& b)
    {
        return a.whiteBalance == b.whiteBalance &&
            a.exposure == b.exposure &&
            a.contrast == b.contrast &&
            a.highlights == b.highlights &&
            a.shadows == b.shadows &&
            a.whites == b.whites &&
            a.blacks == b.blacks &&
            a.hue == b.hue &&
            a.saturation == b.saturation &&
            a.redValue == b.redValue &&
            a.greenValue == b.greenValue &&
            a.blueValue == b.blueValue &&
            a.blur == b.blur &&
            a.blurX == b.blurX &&
            a.blurY == b.blurY &&
            a.sharpness == b.sharpness &&
            a.vignette == b.vignette;
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
    static bool getFilePath(char* path, char* filter)
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
            strcpy(path, ofn.lpstrFile);
            return true;
        }

        return false;
    }
    static bool getSavePath(char* path, const char* filter)
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
            strcpy(path, ofn.lpstrFile);
            output.format = ofn.nFilterIndex;
            return true;
        }

        return false;
    }
    static void updateMetadata()
    {
        //Calculate size on disk
        std::ifstream file(imagePath, std::ios::binary);
        file.seekg(0, std::ios::end); // Seek to the end of the file
        metadata.sizeOnDisk = file.tellg(); // Get the current position (file size)
        file.close();

        //Get file name and folder
        strcpy(metadata.folder, imagePath);
        char* p = metadata.folder;
        while (*p != '\0') p++;
        while (*p != '\\') p--;
        p++;
        strcpy(metadata.name, p);
        *p = 0;

        //Get image dimensions
        metadata.width = img.cols;
        metadata.height = img.rows;
    }
    static void changeTargetImage()
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
        updateMetadata();
        updatePending = true;
        savePending = true;
    }
    static bool readPhedFile()
    {
        std::ifstream file(phedFilePath); // Open the file for reading
        file.read(phedFileContent, MAX_PATH + 68 + 1);  // Read up to MAX_PATH + 68 + 1 characters into target
        phedFileContent[file.gcount()] = '\0';  // Null-terminate the target string
        file.close(); // Close the file

        int cursor = 0;
        if (*phedFileContent == '0')
        {
            *imagePath = '\0';
            cursor = 1;
        }
        else
        {
            //Extract image path
            while (phedFileContent[cursor] != '\n')
            {
                imagePath[cursor] = phedFileContent[cursor];
                cursor++;
            }
            imagePath[cursor] = '\0';
            std::ifstream file(imagePath);
            if (!file.good())
            {
                file.close();
                *imagePath = '\0';
            }
        }

        //Extract editor values
        cursor++;
        current.whiteBalance = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.exposure = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.contrast = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.highlights = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.shadows = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.whites = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.blacks = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.hue = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.saturation = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.redValue = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.greenValue = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.blueValue = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.blur = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.blurX = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.blurY = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.sharpness = atoi(&phedFileContent[cursor]);
        while (phedFileContent[cursor] != '\n') cursor++;
        cursor++;
        current.vignette = atoi(&phedFileContent[cursor]);

        return *imagePath;
    }
    static void writePhedFile(char* path)
    {
        std::ofstream file(phedFilePath);
        if (*imagePath) file << imagePath << '\n';
        else file << "0\n";
        file << current.whiteBalance << '\n';
        file << current.exposure << '\n';
        file << current.contrast << '\n';
        file << current.highlights << '\n';
        file << current.shadows << '\n';
        file << current.whites << '\n';
        file << current.blacks << '\n';
        file << current.hue << '\n';
        file << current.saturation << '\n';
        file << current.redValue << '\n';
        file << current.greenValue << '\n';
        file << current.blueValue << '\n';
        file << current.blur << '\n';
        file << current.blurX << '\n';
        file << current.blurY << '\n';
        file << current.sharpness << '\n';
        file << current.vignette << '\n';
        file.close();
    }
    static bool openFileButtonClicked()
    {
        if (getFilePath(phedFilePath, "PhotoEditor file(.phed)\0 * .phed\0"))
        {
            if (readPhedFile())
            {
                changeTargetImage();
            }
            previous = current;
            savePending = false;
            return true;
        }
        return false;
    }
    static bool saveAsButtonClicked()
    {
        if (getSavePath(phedFilePath, "PhotoEditor file(.phed)\0 * .phed\0\0"))
        {
            writePhedFile(phedFilePath);
            savePending = false;
            return true;
        }
        return false;
    }
    static bool saveButtonClicked()
    {
        if (*phedFilePath)
        {
            //save in existing path
            writePhedFile(phedFilePath);
            savePending = false;
            return true;
        }
        return saveAsButtonClicked();
    }
    static bool newFileButtonClicked()
    {
        if (savePending)
        {
            savePendingWindow = true; //Open save pending window
            return false;
        }
        imagePath[0] = '\0';
        return true;
    }
    static bool logsFolderExists(const char* path)
    {
        DWORD attributes = GetFileAttributes(path);
        return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
    }
    static void createLogsFolder(const char* path)
    {
        std::string logsPath = std::string(path) + "\\Logs";

        // Check if the Logs folder exists
        if (logsFolderExists(logsPath.c_str()))
        {
            // Remove the folder and its contents
            SHFILEOPSTRUCT fileOp = { 0 };
            fileOp.wFunc = FO_DELETE;
            fileOp.pFrom = logsPath.c_str();
            fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

            int result = SHFileOperation(&fileOp);
            if (result != 0) {
                std::cerr << "Failed to remove existing Logs folder: " << logsPath << " (Error " << result << ")" << std::endl;
                return;
            }
        }

        // Create a fresh Logs folder
        if (CreateDirectory(logsPath.c_str(), NULL)) {
            std::cout << "Logs folder created successfully at: " << logsPath << std::endl;
        }
        else {
            std::cerr << "Failed to create Logs folder at: " << logsPath << " (Error " << GetLastError() << ")" << std::endl;
        }
    }
    static void updateHistoryList()
    {
        //std::ofstream file(path, std::ios::out);
    }
    static void changeMade()
    {
        updateHistoryList();
        savePending = true;
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
                if (current.contrast > 0)
                {
                    pixel = (100 - current.contrast) * p[2] / 100.0f;
                    if (pixel > 127.5f - p[2])
                    {
                        pixel = (100 - current.contrast) * (p[2] - 255) / 100.0f + 255;
                        if (pixel < 382.5f - p[2])
                        {
                            pixel = 100 * (p[2] - 127.5f) / (100 - current.contrast) + 127.5f;
                        }
                    }
                    pixel = p[2] * (float)pixel / p[2];
                    p[2] = pixel * (pixel <= 255) + 255 * (pixel > 255);
                }
                else if (current.contrast < 0)
                {
                    p[2] = (p[2] - 127.5f) * (current.contrast / 100.0f + 1) + 127.5f;
                }
                if (current.highlights || current.whites || current.shadows || current.blacks)
                {
                    pixel = p[2];
                    if (p[2] > 200)
                    {
                        //Highlights
                        if (current.highlights)
                        {
                            pixel = p[2] + (p[2] - 200) * (current.highlights) / 20.0f;
                            pixel = pixel * (pixel > 0);
                            pixel = pixel * (pixel <= 255) + 255 * (pixel > 255);
                        }
                        //Whites
                        if (current.whites && p[1] < 40)
                        {
                            pixel = pixel + (p[2] - 200) * (current.whites) / 20.0f;
                            pixel = pixel * (pixel > 0);
                            pixel = pixel * (pixel <= 255) + 255 * (pixel > 255);
                        }
                    }
                    else if (p[2] < 128)
                    {
                        //Shadows
                        if (current.shadows)
                        {
                            pixel = p[2] + (127 - p[2]) * (current.shadows) / 40.0f;
                            pixel = pixel * (pixel > 0);
                            pixel = pixel * (pixel <= 255) + 255 * (pixel > 255);
                        }
                        //Blacks
                        if (current.blacks && p[2] < 60)
                        {
                            pixel = pixel + (60 - p[2]) * (current.blacks) / 40.0f;
                            pixel = pixel * (pixel > 0);
                            pixel = pixel * (pixel <= 255) + 255 * (pixel > 255);
                        }
                    }

                    p[2] = pixel;
                }
                if (current.hue)
                {
                    unsigned char hueOffset = current.hue * 2.55f;
                    *p += hueOffset;
                }
                if (current.saturation)
                {
                    float multiplier = (current.saturation + 100) / 100.0f;
                    if (current.saturation > 0) multiplier *= multiplier;
                    unsigned short value = p[1] * multiplier;
                    value = (value <= 255) * value + (value > 255) * 255;
                    p[1] = value;
                }
                if (current.vignette)
                {
                    unsigned threshold = maxDistance / 2;
                    unsigned distance = sqrt((h - imageHeight / 2) * (h - imageHeight / 2) + (w - imageWidth / 2) * (w - imageWidth / 2));

                    if (distance > threshold)
                    {
                        //Adjust Value
                        pixel = p[2] + current.vignette * ((float)(distance - threshold) / maxDistance) * 10;
                        pixel *= (pixel > 0);
                        p[2] = (pixel <= 255) * pixel + (pixel > 255) * 255;

                        //Adjust saturation
                        if (current.vignette > 0)
                        {
                            pixel = p[1] - current.vignette * ((float)(distance - threshold) / maxDistance) * 10;
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
        std::vector<cv::Mat> channels;
        //img is original image, imgRGB is edited image
        imgRGB = img.clone();

        split(imgRGB, channels); // Channels[0] is red, [1] is green, [2] is blue

        //White balance
        //Amber(Hot temperature): rgb(255, 191, 0)
        //So cold temperature is: rgb(0, 64, 255)
        if (current.whiteBalance > 0)
        {
            channels[0] = ((100 + current.whiteBalance) / 100.0f) * channels[0];
            channels[1] = (-current.whiteBalance / 100.0f) * 64 + ((100 + current.whiteBalance) / 100.0f) * channels[1];
            channels[2] = (-current.whiteBalance / 100.0f) * 255 + ((100 + current.whiteBalance) / 100.0f) * channels[2];
        }
        else if (current.whiteBalance < 0)
        {
            channels[0] = ((100 + current.whiteBalance) / 100.0f) * channels[0];
            channels[1] = (current.whiteBalance / 100.0f) * 191 + ((100 - current.whiteBalance) / 100.0f) * channels[1];
            channels[2] = ((100 - current.whiteBalance) / 100.0f) * channels[2];
        }


        //Value, exposure
        totalRedValue = current.redValue + current.exposure;
        channels[0] *= totalRedValue / 100.0f;
        totalGreenValue = current.greenValue + current.exposure;
        channels[1] *= totalGreenValue / 100.0f;
        totalBlueValue = current.blueValue + current.exposure;
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
        int totalBlurX = current.blur + current.blurX;
        int totalBlurY = current.blur + current.blurY;
        if (totalBlurX || totalBlurY)
        {
            cv::GaussianBlur(imgRGB, imgRGB, cv::Size(totalBlurX * 2 + 1, totalBlurY * 2 + 1), 0, 0);
        }

        //Sharpness
        if (current.sharpness)
        {
            float sharpness = (current.sharpness / (1.0f * (current.sharpness > 0) + 4.0f * (current.sharpness < 0)) + 1) / 4;
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
        #pragma region Thread setup
        numThreads = std::thread::hardware_concurrency();
        if (!numThreads)
        {
            numThreads = 1;
        }
        else if (numThreads > 3)
        {
            numThreads--;
        }
        #pragma endregion

        #pragma region Icon setup
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
        #pragma endregion

        #pragma region Work image setup
        //img = cv::imread(R"(..\..\..\sample image.jpg)", -1);
        //cv::cvtColor(img, imgRGB, cv::COLOR_BGRA2RGBA);
        //img = imgRGB.clone();
        //imageWidth = img.cols;
        //imageHeight = img.rows;
        //originalImageData = matToTexture(imgRGB);
        //originalImageHandle = (GLuint)(intptr_t)originalImageData;
        //targetImageData = matToTexture(imgRGB);
        //targetImageHandle = (GLuint)(intptr_t)targetImageData;
        //strcpy(imagePath, R"(..\..\..\sample image.jpg)");
        //updateMetadata();
        #pragma endregion

        //createLogsFolder("C:\\Users\\parth\\Desktop\\Temporary\\trial");


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
            newFileButtonClicked();
            homePage = false;
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
            homePage = !openFileButtonClicked();
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
            savePending = true;
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
            settingsWindow = true;
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
        if (ImGui::Begin("Main Window", NULL, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | (savePending * ImGuiWindowFlags_UnsavedDocument)))
        {
            if (*imagePath)
            {
                ImGui::Text("Debug text");
                if (updateStage != 0 && updateTime > 200)
                    ImGui::Text("Update time: %d ms (Processing...)", updateTime);
                else
                    ImGui::Text("Update time: %d ms", updateTime);

                if (ImGui::Button("Random sliders"))
                {
                    current = random;
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
            }
            else
            {
                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() / 2 - 225 SC, ImGui::GetWindowHeight() / 2 - 50 SC));
                ImGui::PushFont(head);
                if (ImGui::Button("Open image", ImVec2(450 SC, 100 SC)))
                {
                    if (getFilePath(imagePath, "JPG image\0*.jpg;\0PNG image\0*.png;\0WEBP image\0*.webp;\0TIF image\0*.tif;*.tiff\0"))
                    {
                        changeTargetImage();
                    }
                }
                ImGui::PopFont();
            }

            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 49 SC);
            if (*imagePath)
            {
                ImGui::BeginChild("Stastistics", ImVec2(0, 0), true);
                if (ImGui::Button("Reset view"))
                {
                    posX = 20;
                    posY = 20;
                    zoomFactor = 100;
                }
                ImGui::SameLine();
                ImGui::Text(" Zoom : ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(250 SC);
                ImGui::SliderFloat(" ", &zoomFactor, 5.0f, 1000.0f, "%.4f%%", ImGuiSliderFlags_Logarithmic);
                ImGui::SameLine();
                ImGui::Text("Position : (%dpx, %dpx)", posX, posY);
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(4, 4));
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 118 SC);
                if (ImGui::Button("Export image"))
                {
                    exportWindow = getSavePath(output.path, "JPG image\0*.jpg\0PNG image\0*.png\0WEBP image\0*.webp\0TIF image\0*.tif;*.tiff\0\0");
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
        if (ImGui::Begin("Sliders"))
        {
            if (ImGui::Button("Reset sliders"))
            {
                current = default;
                updatePending = true;
            }
            ImGui::Dummy(ImVec2(1, 10 SC));
            sliderWidth = ImGui::GetWindowWidth() - 20 SC;
            ImGui::PushItemWidth(sliderWidth);
            for (int sliderCount = 0; sliderCount < sliderStringCount; sliderCount++)
            {
                ImGui::Text(sliderStrings + sliderCount * jumpLength);
                char temp[5] = "##";
                itoa(sliderCount, temp + 2, 10);
                updatePending = ImGui::SliderInt(temp, (int*)&current + sliderCount, sliderMin[sliderCount],
                    sliderMax[sliderCount], "%d", ImGuiSliderFlags_AlwaysClamp) || updatePending;
                if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("CTRL+Click to input using keyboard\nRight click to reset to default");
                    ImGui::EndTooltip();
                }
                ImGui::Dummy(ImVec2(1, 10 SC));
            }
            ImGui::PopItemWidth();
            ImGui::End();
        }
        if (ImGui::Begin("Image data"))
        {
            if (*imagePath)
            {
                ImGui::Text("Name: ");
                ImGui::SameLine();
                ImGui::Text(metadata.name);
                ImGui::Text("Folder: ");
                ImGui::SameLine();
                ImGui::Text(metadata.folder);
                ImGui::Text("Size: ");
                ImGui::SameLine();
                ImGui::Text("%lu", metadata.sizeOnDisk);
                ImGui::SameLine();
                ImGui::Text("Bytes");
                ImGui::Text("Resolution: ");
                ImGui::SameLine();
                ImGui::Text("%d x %d", metadata.width, metadata.height);
                ImGui::Text("Preview: ");
                previewWidth = ImGui::GetWindowWidth() - 16 SC;
                if (previewWidth > 200 SC)
                    previewWidth = 200 SC;
                previewHeight = (float)previewWidth * imageHeight / imageWidth;
                ImGui::Image(originalImageData, ImVec2(previewWidth, previewHeight), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0.5f, 0.5f, 0.5f, 1));
                if (ImGui::Button("Replace image"))
                {
                    if (getFilePath(imagePath, "JPG image\0*.jpg;\0PNG image\0*.png;\0WEBP image\0*.webp;\0TIF image\0*.tif;*.tiff\0"))
                    {
                        changeTargetImage();
                    }
                }
                if (ImGui::Button("Remove image"))
                {
                    *imagePath = '\0';
                    savePending = true;
                }
            }
            else
            {
                ImGui::TextWrapped("No image is linked to this PHED file.");
                if (ImGui::Button("Open image"))
                {
                    if (getFilePath(imagePath, "JPG image\0*.jpg;\0PNG image\0*.png;\0WEBP image\0*.webp;\0TIF image\0*.tif;*.tiff\0"))
                    {
                        changeTargetImage();
                    }
                }
            }
            ImGui::End();
        }

        if (exportWindow)
        {
            ImGui::Begin("Export window", &exportWindow, ImGuiWindowFlags_NoDocking);
            switch (output.format)
            {
            case 1:
                ImGui::TextWrapped("JPG export quality:\nHigher quality means less compression");
                ImGui::SliderInt("##25", &output.jpgQuality, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
                exportParameters = { cv::IMWRITE_JPEG_QUALITY, output.jpgQuality };
                ImGui::SameLine();
                if (ImGui::Button("Reset"))
                {
                    output.jpgQuality = 95;
                }
                break;
            case 2:
                ImGui::TextWrapped("PNG export compression:\nHigher compression means lesser quality");
                ImGui::SliderInt("##26", &output.pngCompression, 0, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
                exportParameters = { cv::IMWRITE_PNG_COMPRESSION, output.pngCompression };
                ImGui::SameLine();
                if (ImGui::Button("Reset"))
                {
                    output.pngCompression = 3;
                }
                break;
            case 3:
                ImGui::TextWrapped("WEBP export quality:\nHigher quality means less compression");
                ImGui::SliderInt("##27", &output.webpQuality, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
                exportParameters = { cv::IMWRITE_WEBP_QUALITY, output.webpQuality };
                ImGui::SameLine();
                if (ImGui::Button("Reset"))
                {
                    output.webpQuality = 75;
                }
                break;
            case 4:
                ImGui::TextWrapped("TIF compression algorithm:");
                ImGui::RadioButton("No compression", &output.tifCompression, 1);
                ImGui::RadioButton("CCITT Group 3", &output.tifCompression, 2);
                ImGui::RadioButton("CCITT Group 4", &output.tifCompression, 3);
                ImGui::RadioButton("LZW", &output.tifCompression, 5);
                ImGui::RadioButton("JPEG compression", &output.tifCompression, 7);
                exportParameters = { cv::IMWRITE_TIFF_COMPRESSION, output.tifCompression };
                ImGui::SameLine();
                if (ImGui::Button("Reset"))
                {
                    output.tifCompression = 1;
                }
                break;
            }
            if (ImGui::Button("Export"))
            {
                cv::Mat imgExport;
                cv::cvtColor(imgRGB, imgExport, cv::COLOR_RGBA2BGRA);
                cv::imwrite(output.path, imgExport, exportParameters);
                exportSuccessTimer = clock();
            }
            if (difftime(clock(), exportSuccessTimer) < 3000)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                ImGui::TextWrapped("Image exported successfully");
                ImGui::PopStyleColor();
            }

            ImGui::End();
        }
        if (savePendingWindow)
        {
            ImGui::Begin("Save pending", &savePendingWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextWrapped("There are unsaved changes in the current file.\nDo you want to save them?");
            ImGui::Dummy(ImVec2(1, 20 SC));
            if (ImGui::Button("Save file"))
            {
                savePendingWindow = !saveButtonClicked();

            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 100 SC);
            if (ImGui::Button("Don't save"))
            {
                savePendingWindow = false;
                imagePath[0] = '\0';
                savePending = true;
            }
            ImGui::End();
        }

        if (!io.MouseDown[0] && prevMouseDown)
        {
            if (!isEditorEqual(previous, current))
            {
                changeMade();
            }
            previous = current;
        }
        else if (!isEditorEqual(previous, current) && !io.MouseDown[0])
        {
            changeMade();
        }
        prevMouseDown = io.MouseDown[0];
    }

    void renderUI(bool& exit, float& scale, ImFont* head, ImFont* subhead, ImGuiIO& io)
    {
        ImGui::DockSpaceOverViewport(ImGui::GetWindowDockID(), 0);
        // Vsync, reduce fps when mouse not moving
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
                    if (ImGui::MenuItem("New", "Ctrl+N"))
                    {
                        newFileButtonClicked();
                    }
                    if (ImGui::MenuItem("Open", "Ctrl+O"))
                    {
                        if (savePending) savePendingWindow = true;
                        else openFileButtonClicked();
                    }
                    if (ImGui::MenuItem("Save", "Ctrl+S"))
                    {
                        saveButtonClicked();
                    }
                    if (ImGui::MenuItem("Save As", "Ctrl+Shift+S"))
                    {
                        saveAsButtonClicked();
                    }
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

        if (settingsWindow)
        {
            ImGui::Begin("Settings", &settingsWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings);
            ImGui::Text("Settings will be added soon");
            ImGui::Button("Save settings");
            ImGui::Button("Reset all");
            ImGui::End();
        }

    }

}
