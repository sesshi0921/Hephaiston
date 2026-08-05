#pragma once

#include "hephaiston/EditorTypes.h"
#include "hephaiston/Framebuffer.h"

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

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
    void render(ViewMode mode, const ViewportStatus& status, const ViewportRenderSettings& settings, const std::vector<std::unique_ptr<IViewportSceneLayer>>& sceneLayers);

    [[nodiscard]] unsigned int texture() const { return framebuffer_.texture(); }
    [[nodiscard]] int width() const { return framebuffer_.width(); }
    [[nodiscard]] int height() const { return framebuffer_.height(); }

private:
    void createPipeline();
    void destroyPipeline();
    void updateSceneGeometry(ViewMode mode, const ViewportStatus& status, const ViewportRenderSettings& settings, const std::vector<std::unique_ptr<IViewportSceneLayer>>& sceneLayers);
    void drawTexturedMeshes(const std::vector<ViewportTexturedMesh>& meshes, const float* mvp);
    [[nodiscard]] unsigned int compileShader(unsigned int type, const char* source) const;
    [[nodiscard]] unsigned int linkProgram(unsigned int vertexShader, unsigned int fragmentShader) const;

    Framebuffer framebuffer_;
    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int pointVao_ = 0;
    unsigned int pointVbo_ = 0;
    unsigned int texturedProgram_ = 0;
    unsigned int texturedVao_ = 0;
    unsigned int texturedVbo_ = 0;
    unsigned int texturedEbo_ = 0;
    int vertexCount_ = 0;
    int pointCount_ = 0;
    std::vector<ViewportTexturedMesh> texturedMeshes_;
    std::unordered_map<std::string, unsigned int> textures_;
};

} // namespace hephaiston
