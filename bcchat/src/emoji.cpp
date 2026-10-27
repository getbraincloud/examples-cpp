#include "emoji.h"
#include "emojiShortcodes.generated.h"

#include <algorithm>
#include <cstring>

std::string emoji_expandShortcodes(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    size_t cursor = 0;
    while (cursor < text.size())
    {
        size_t start = text.find(':', cursor);
        if (start == std::string::npos)
        {
            result.append(text, cursor, std::string::npos);
            break;
        }
        result.append(text, cursor, start - cursor);
        size_t end = text.find(':', start + 1);
        if (end == std::string::npos)
        {
            result.append(text, start, std::string::npos);
            break;
        }
        std::string name = text.substr(start + 1, end - start - 1);
        bool validName = !name.empty() && name.size() <= 64 &&
            std::all_of(name.begin(), name.end(), [](unsigned char character)
            {
                return (character >= 'a' && character <= 'z') ||
                    (character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9') ||
                    character == '_' || character == '+' || character == '-';
            });
        auto found = std::lower_bound(EMOJI_SHORTCODES, EMOJI_SHORTCODES + EMOJI_SHORTCODE_COUNT,
            name.c_str(), [](const EmojiShortcodeEntry& entry, const char* value)
            {
                return std::strcmp(entry.alias, value) < 0;
            });
        if (!validName || found == EMOJI_SHORTCODES + EMOJI_SHORTCODE_COUNT || name != found->alias)
        {
            // Preserve an unknown colon and continue scanning so URL schemes
            // and unknown tokens cannot hide a later valid shortcode.
            result.push_back(':');
            cursor = start + 1;
            continue;
        }
        else
            result += found->emoji;
        cursor = end + 1;
    }
    return result;
}
