#pragma once

#include "hephaiston/EditorTypes.h"

#include <algorithm>
#include <string_view>
#include <vector>

namespace hephaiston {

class SceneRegistry {
public:
    void clear() { rootObjects_.clear(); }
    void addRootObject(SceneObject object) { rootObjects_.push_back(std::move(object)); }

    [[nodiscard]] std::vector<SceneObject>& rootObjects() { return rootObjects_; }
    [[nodiscard]] const std::vector<SceneObject>& rootObjects() const { return rootObjects_; }

    [[nodiscard]] SceneObject* find(std::string_view id) {
        for (auto& object : rootObjects_) {
            if (SceneObject* found = findRecursive(object, id)) {
                return found;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const SceneObject* find(std::string_view id) const {
        for (const auto& object : rootObjects_) {
            if (const SceneObject* found = findRecursive(object, id)) {
                return found;
            }
        }
        return nullptr;
    }

private:
    static SceneObject* findRecursive(SceneObject& object, std::string_view id) {
        if (object.id == id) {
            return &object;
        }
        for (auto& child : object.children) {
            if (SceneObject* found = findRecursive(child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    static const SceneObject* findRecursive(const SceneObject& object, std::string_view id) {
        if (object.id == id) {
            return &object;
        }
        for (const auto& child : object.children) {
            if (const SceneObject* found = findRecursive(child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    std::vector<SceneObject> rootObjects_;
};

} // namespace hephaiston
