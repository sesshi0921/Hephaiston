#include "hephaiston/Framebuffer.h"

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <stdexcept>
#include <utility>

namespace hephaiston {

Framebuffer::~Framebuffer() {
    destroy();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept {
    *this = std::move(other);
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        framebuffer_ = std::exchange(other.framebuffer_, 0);
        colorTexture_ = std::exchange(other.colorTexture_, 0);
        depthStencil_ = std::exchange(other.depthStencil_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

void Framebuffer::resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width == width_ && height == height_ && valid()) {
        return;
    }

    destroy();
    width_ = width;
    height_ = height;

    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);

    glGenRenderbuffers(1, &depthStencil_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencil_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        destroy();
        throw std::runtime_error("Failed to create complete framebuffer");
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

void Framebuffer::bindDefault() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::destroy() {
    if (depthStencil_ != 0) {
        glDeleteRenderbuffers(1, &depthStencil_);
        depthStencil_ = 0;
    }
    if (colorTexture_ != 0) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    if (framebuffer_ != 0) {
        glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

} // namespace hephaiston
