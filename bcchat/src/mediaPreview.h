#pragma once

#include <string>
#include <vector>

// Finds http(s) links in a message, without trailing sentence punctuation.
std::vector<std::string> mediaPreview_findUrls(const std::string& text);

// True for known image/GIF URLs or URLs whose downloaded content has been
// identified as media.
bool mediaPreview_isDirectMediaUrl(const std::string& url);

// Draws an inline preview for a URL. Image/GIF URLs are downloaded and shown;
// other URLs get a compact link card (YouTube cards also include a thumbnail).
void mediaPreview_draw(const std::string& url);

// Releases cached downloads and GPU textures before the renderer shuts down.
void mediaPreview_shutdown();

// Adds Unicode symbol/emoji glyphs to ImGui's font atlas where available.
void mediaPreview_initializeFonts();
