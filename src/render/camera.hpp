#pragma once
#include <cmath>

class Camera {
public:
    void pan(glm::vec2 dt) {
        offsetX -= dt.x / scale;
        offsetY -= dt.y / scale;
    }

    void zoom(float scrollOffset, double mouseX, double mouseY, int screenWidth, int screenHeight) {
        float halfWidth = static_cast<float>(screenWidth) * 0.5f;
        float halfHeight = static_cast<float>(screenHeight) * 0.5f;

        const float mouseWorldX = (static_cast<float>(mouseX) - halfWidth) / scale + offsetX;
        const float mouseWorldY = (static_cast<float>(mouseY) - halfHeight) / scale + offsetY;

        static constexpr float zoomFactor = 1.1f;
        if(scrollOffset > 0)
            scale *= zoomFactor;
        else
            scale /= zoomFactor;

        scale = std::clamp(scale, 0.1f, 64.0f);

        offsetX = mouseWorldX - (static_cast<float>(mouseX) - halfWidth) / scale;
        offsetY = mouseWorldY - (static_cast<float>(mouseY) - halfHeight) / scale;
    }

    [[nodiscard]] glm::vec2 getOffset() const { return {offsetX, offsetY}; }

    void setOffset(glm::vec2 offset) {
        offsetX = offset.x;
        offsetY = offset.y;
    }

    [[nodiscard]] float getScale() const { return scale; }
    [[nodiscard]] float getOffsetX() const { return offsetX; }
    [[nodiscard]] float getOffsetY() const { return offsetY; }

private:
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float scale = 4.0f;
};