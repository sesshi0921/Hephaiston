#pragma once

#include "hephaiston/EditorTypes.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace hephaiston {

class SelectionManager {
public:
    void clear() { selection_.clear(); }

    void select(SelectionItem item, bool additive = false) {
        if (!additive) {
            selection_.clear();
        }
        if (!contains(item.id)) {
            selection_.push_back(std::move(item));
        }
    }

    void toggle(SelectionItem item) {
        auto it = std::find_if(selection_.begin(), selection_.end(), [&](const SelectionItem& selected) { return selected.id == item.id; });
        if (it != selection_.end()) {
            selection_.erase(it);
        } else {
            selection_.push_back(std::move(item));
        }
    }

    [[nodiscard]] bool contains(std::string_view id) const {
        return std::any_of(selection_.begin(), selection_.end(), [&](const SelectionItem& item) { return item.id == id; });
    }

    [[nodiscard]] bool empty() const { return selection_.empty(); }
    [[nodiscard]] const std::vector<SelectionItem>& items() const { return selection_; }
    [[nodiscard]] const SelectionItem* primary() const { return selection_.empty() ? nullptr : &selection_.front(); }

private:
    std::vector<SelectionItem> selection_;
};

} // namespace hephaiston
