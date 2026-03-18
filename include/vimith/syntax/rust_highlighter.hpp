#pragma once
#include "vimith/syntax/i_highlighter.hpp"

namespace vimith::syntax {

class RustHighlighter final : public IHighlighter {
public:
    [[nodiscard]] std::string_view language() const noexcept override { return "rust"; }
    void tokenize(std::string_view lineText, HighlightedLine& out) const override;
};

} // namespace vimith::syntax
