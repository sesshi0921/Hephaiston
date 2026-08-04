#include "hephaiston/ViewportRenderer.h"

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hephaiston {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 normalize(Vec3 v) {
    const float len = std::sqrt(std::max(0.000001f, dot(v, v)));
    return {v.x / len, v.y / len, v.z / len};
}

struct Mat4 {
    std::array<float, 16> m {};
};

Mat4 identity() {
    Mat4 r;
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r.m[c * 4 + row] = a.m[0 * 4 + row] * b.m[c * 4 + 0] +
                               a.m[1 * 4 + row] * b.m[c * 4 + 1] +
                               a.m[2 * 4 + row] * b.m[c * 4 + 2] +
                               a.m[3 * 4 + row] * b.m[c * 4 + 3];
        }
    }
    return r;
}

Mat4 orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    Mat4 r = identity();
    r.m[0] = 2.0f / (right - left);
    r.m[5] = 2.0f / (top - bottom);
    r.m[10] = -2.0f / (farPlane - nearPlane);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    return r;
}

Mat4 perspective(float fovyRadians, float aspect, float nearPlane, float farPlane) {
    Mat4 r;
    const float f = 1.0f / std::tan(fovyRadians * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return r;
}

Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, normalize(up)));
    const Vec3 u = cross(s, f);

    Mat4 r = identity();
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    return r;
}

constexpr const char* kVertexShader = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 vColor;
uniform mat4 uMvp;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vColor = aColor;
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)GLSL";

using Color = std::array<float, 3>;

void pushLine(std::vector<float>& vertices, Vec3 a, Vec3 b, Color color) {
    vertices.insert(vertices.end(), {a.x, a.y, a.z, color[0], color[1], color[2], b.x, b.y, b.z, color[0], color[1], color[2]});
}

float niceGridStep(float desiredMeters) {
    desiredMeters = std::max(0.2f, desiredMeters);
    const float power = std::pow(10.0f, std::floor(std::log10(desiredMeters)));
    const float normalized = desiredMeters / power;
    float nice = 1.0f;
    if (normalized >= 5.0f) nice = 5.0f;
    else if (normalized >= 2.0f) nice = 2.0f;
    return std::clamp(nice * power, 0.2f, 10000000.0f);
}

} // namespace

ViewportRenderer::ViewportRenderer() {
    createPipeline();
}

ViewportRenderer::~ViewportRenderer() {
    destroyPipeline();
}

void ViewportRenderer::resize(int width, int height) {
    framebuffer_.resize(width, height);
}

void ViewportRenderer::render(ViewMode mode, const ViewportStatus& status, const ViewportRenderSettings& settings, const std::vector<std::unique_ptr<IViewportSceneLayer>>& sceneLayers) {
    if (!framebuffer_.valid()) {
        return;
    }

    updateSceneGeometry(mode, status, settings, sceneLayers);

    framebuffer_.bind();
    glViewport(0, 0, framebuffer_.width(), framebuffer_.height());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    if (mode == ViewMode::Mode3D) {
        glClearColor(0.040f, 0.052f, 0.068f, 1.0f);
    } else {
        glClearColor(0.060f, 0.064f, 0.070f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = framebuffer_.height() > 0 ? static_cast<float>(framebuffer_.width()) / static_cast<float>(framebuffer_.height()) : 1.0f;
    Mat4 mvp;
    if (mode == ViewMode::Mode2D) {
        const float metersPerPixel = static_cast<float>(std::max(0.005, status.metersPerPixel));
        const float halfWidth = static_cast<float>(framebuffer_.width()) * metersPerPixel * 0.5f;
        const float halfHeight = static_cast<float>(framebuffer_.height()) * metersPerPixel * 0.5f;
        mvp = orthographic(status.panMeters.x - halfWidth, status.panMeters.x + halfWidth,
                           status.panMeters.y - halfHeight, status.panMeters.y + halfHeight,
                           -100.0f, 100.0f);
    } else {
        const float yaw = status.orbitYawDegrees * kDegToRad;
        const float pitch = status.orbitPitchDegrees * kDegToRad;
        const float distance = std::max(2.0f, status.orbitDistanceMeters);
        const Vec3 target {status.targetX, status.targetY, status.targetZ};
        const Vec3 eye {
            target.x + distance * std::cos(pitch) * std::cos(yaw),
            target.y + distance * std::cos(pitch) * std::sin(yaw),
            target.z + distance * std::sin(pitch),
        };
        const float farPlane = std::max(1000.0f, distance * 4.0f + 1000.0f);
        const Mat4 projection = perspective(50.0f * kDegToRad, aspect, 0.05f, farPlane);
        const Mat4 view = lookAt(eye, target, {0.0f, 0.0f, 1.0f});
        mvp = multiply(projection, view);
    }

    glUseProgram(program_);
    glUniformMatrix4fv(glGetUniformLocation(program_, "uMvp"), 1, GL_FALSE, mvp.m.data());

    glBindVertexArray(vao_);
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, vertexCount_);
    glBindVertexArray(0);
    glUseProgram(0);

    Framebuffer::bindDefault();
}

void ViewportRenderer::createPipeline() {
    const unsigned int vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const unsigned int fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    program_ = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);
}

void ViewportRenderer::destroyPipeline() {
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

void ViewportRenderer::updateSceneGeometry(ViewMode mode, const ViewportStatus& status, const ViewportRenderSettings& settings, const std::vector<std::unique_ptr<IViewportSceneLayer>>& sceneLayers) {
    std::vector<float> vertices;
    vertices.reserve(12000);

    const Color minor {0.18f, 0.21f, 0.24f};
    const Color major {0.28f, 0.32f, 0.36f};
    const Color xAxis {0.90f, 0.25f, 0.25f};
    const Color yAxis {0.24f, 0.80f, 0.34f};
    const Color zAxis {0.35f, 0.58f, 1.00f};

    const int gridHalfCount = mode == ViewMode::Mode2D ? 160 : 200;
    const float gridStep = mode == ViewMode::Mode2D
                               ? static_cast<float>(std::max(0.25, status.gridSizeMeters))
                               : niceGridStep(status.orbitDistanceMeters / 35.0f);
    const float gridMin = -static_cast<float>(gridHalfCount) * gridStep;
    const float gridMax = static_cast<float>(gridHalfCount) * gridStep;

    if (settings.showHorizontalGrid) {
        for (int i = -gridHalfCount; i <= gridHalfCount; ++i) {
            const float p = static_cast<float>(i) * gridStep;
            const bool isAxis = i == 0;
            const bool isMajor = i % 5 == 0;
            pushLine(vertices, {p, gridMin, 0.0f}, {p, gridMax, 0.0f}, isAxis ? yAxis : (isMajor ? major : minor));
            pushLine(vertices, {gridMin, p, 0.0f}, {gridMax, p, 0.0f}, isAxis ? xAxis : (isMajor ? major : minor));
        }
    }

    if (settings.showOriginAxes && mode == ViewMode::Mode3D) {
        // Minimal orientation axis only. Add-ons will provide actual parcels/buildings later.
        const float axisLength = std::max(8.0f, gridStep * 4.0f);
        pushLine(vertices, {0.0f, 0.0f, 0.0f}, {axisLength, 0.0f, 0.0f}, xAxis);
        pushLine(vertices, {0.0f, 0.0f, 0.0f}, {0.0f, axisLength, 0.0f}, yAxis);
        pushLine(vertices, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, axisLength}, zAxis);
    }

    std::vector<ViewportLine> pluginLines;
    for (const auto& layer : sceneLayers) {
        if (layer && layer->visible()) {
            layer->collectViewportLines(pluginLines);
        }
    }
    for (const auto& line : pluginLines) {
        pushLine(vertices,
                 {line.x1, line.y1, line.z1},
                 {line.x2, line.y2, line.z2},
                 {line.color.x, line.color.y, line.color.z});
    }

    vertexCount_ = static_cast<int>(vertices.size() / 6);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

unsigned int ViewportRenderer::compileShader(unsigned int type, const char* source) const {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024] = {};
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        glDeleteShader(shader);
        std::ostringstream oss;
        oss << "OpenGL shader compile failed: " << infoLog;
        throw std::runtime_error(oss.str());
    }
    return shader;
}

unsigned int ViewportRenderer::linkProgram(unsigned int vertexShader, unsigned int fragmentShader) const {
    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024] = {};
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        glDeleteProgram(program);
        std::ostringstream oss;
        oss << "OpenGL program link failed: " << infoLog;
        throw std::runtime_error(oss.str());
    }
    return program;
}

} // namespace hephaiston
