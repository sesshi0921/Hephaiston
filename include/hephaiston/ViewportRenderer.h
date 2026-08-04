#pragma once

#include "hephaiston/EditorTypes.h"
#include "hephaiston/Framebuffer.h"

#include <string>

namespace hephaiston {

class ViewportRenderer {
public:
    ViewportRenderer();
    ~ViewportRenderer();

    ViewportRenderer(const ViewportRenderer&) = delete;
    ViewportRenderer& operator=(const ViewportRenderer&) = delete;
    ViewportRenderer(ViewportRenderer&&) = delete;
    ViewportRenderer& operator=(ViewportRenderer&&) = delete;

    void resize(int width, int height);
    void render(ViewMode mode, const ViewportStatus& status);

    [[nodiscard]] unsigned int texture() const { return framebuffer_.texture(); }
    [[nodiscard]] int width() const { return framebuffer_.width(); }
    [[nodiscard]] int height() const { return framebuffer_.height(); }

private:
    void createPipeline();
    void destroyPipeline();
    void updateSceneGeometry(ViewMode mode, const ViewportStatus& status);
    [[nodiscard]] unsigned int compileShader(unsigned int type, const char* source) const;
    [[nodiscard]] unsigned int linkProgram(unsigned int vertexShader, unsigned int fragmentShader) const;

    Framebuffer framebuffer_;
    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int vertexCount_ = 0;
};

} // namespace hephaiston
