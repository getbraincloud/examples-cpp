#pragma once

#include <string>

// Converts supported :shortcode: tokens to their UTF-8 emoji equivalents.
// Unknown tokens are preserved verbatim.
std::string emoji_expandShortcodes(const std::string& text);
