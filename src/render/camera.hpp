#pragma once

class Camera {
public:
    void Pan(float dx, float dy) {
        offsetX -= dx / scale;
        offsetY -= dy / scale;
    }

    void Zoom(float scrollOffset, double mouseX, double mouseY) {
        const float mouseWorldX =
            (static_cast<float>(mouseX) / scale) + offsetX;
        const float mouseWorldY =
            (static_cast<float>(mouseY) / scale) + offsetY;

        static constexpr float zoomFactor = 1.1f;
        if(scrollOffset > 0) scale *= zoomFactor;
        else scale /= zoomFactor;

        scale = std::clamp(scale, 0.01f, 64.0f);

        offsetX = mouseWorldX - (static_cast<float>(mouseX) / scale);
        offsetY = mouseWorldY - (static_cast<float>(mouseY) / scale);
    }

    [[nodiscard]] float GetScale() const { return scale; }
    [[nodiscard]] float GetOffsetX() const { return offsetX; }
    [[nodiscard]] float GetOffsetY() const { return offsetY; }

private:
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float scale = 4.0f;
};