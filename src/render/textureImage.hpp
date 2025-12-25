#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class TextureImage {
public:
    TextureImage(const std::string& filepath) {
        data = stbi_load(filepath.c_str(), &width, &height, &channels,
                         4);
    }

    ~TextureImage() {
        if(data) {
            stbi_image_free(data);
        }
    }

    TextureImage(const TextureImage&) = delete;
    TextureImage& operator=(const TextureImage&) = delete;

    TextureImage(TextureImage&& other) noexcept
        : data(other.data), width(other.width),
          height(other.height), channels(other.channels) {
        other.data = nullptr;
    }

    [[nodiscard]] bool isValid() const { return data != nullptr; }
    [[nodiscard]] int getWidth() const { return width; }
    [[nodiscard]] int getHeight() const { return height; }
    [[nodiscard]] const unsigned char* getData() const { return data; }

private:
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
};