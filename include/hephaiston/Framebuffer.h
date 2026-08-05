#pragma once

#include <cstdint>

namespace hephaiston {

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    void resize(int width, int height);
    void bind() const;
    static void bindDefault();

    [[nodiscard]] unsigned int texture() const { return colorTexture_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] bool valid() const { return framebuffer_ != 0 && colorTexture_ != 0; }

private:
    void destroy();

    unsigned int framebuffer_ = 0;
    unsigned int colorTexture_ = 0;
    unsigned int depthStencil_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace hephaiston
