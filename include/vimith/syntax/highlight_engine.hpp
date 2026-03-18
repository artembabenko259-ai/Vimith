#pragma once

#include "vimith/syntax/i_highlighter.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vimith::syntax {

// ---------------------------------------------------------------------------
// HighlightEngine
//
// Registry that maps language identifiers to IHighlighter implementations.
// Also performs file-extension → language detection.
// ---------------------------------------------------------------------------
class HighlightEngine {
public:
    HighlightEngine();

    // Register a highlighter plugin.  Replaces any existing plugin for the
    // same language ID.
    void registerHighlighter(std::unique_ptr<IHighlighter> impl);

    // Look up a registered highlighter by language ID.
    // Returns nullptr if no highlighter is registered for that language.
    [[nodiscard]] const IHighlighter* getHighlighter(std::string_view lang) const noexcept;

    // Detect language from file path (extension-based heuristic).
    // Returns empty string if language is unknown.
    [[nodiscard]] static std::string detectLanguage(std::string_view filePath) noexcept;

    // Convenience: detect language from path, then tokenize a line.
    [[nodiscard]] HighlightedLine highlight(std::string_view lineText,
                                             std::string_view filePath) const;

private:
    std::unordered_map<std::string, std::unique_ptr<IHighlighter>> m_registry;
};

} // namespace vimith::syntax
