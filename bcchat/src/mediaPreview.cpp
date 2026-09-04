#include "mediaPreview.h"

#include "imgui.h"
#include "braincloud/internal/URLLoader.h"
#include "braincloud/internal/URLRequest.h"
#include "json/json.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <utility>

#ifndef BCCHAT_UWP
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <gl/GL.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../relaytestapp/src/stb/stb_image.h"
#endif

namespace
{
    const float PREVIEW_MAX_WIDTH = 360.0f;
    const float PREVIEW_MAX_HEIGHT = 240.0f;
    const size_t PREVIEW_MAX_DOWNLOAD_BYTES = 80 * 1024 * 1024;
    const size_t PREVIEW_MAX_DECODED_BYTES = 512 * 1024 * 1024;

    struct DecodedImage
    {
        std::vector<unsigned char> pixels;
        std::vector<int> delays;
        std::string error;
        int sourceWidth = 0;
        int sourceHeight = 0;
        int textureWidth = 0;
        int textureHeight = 0;
        int frameCount = 0;
    };

    struct Preview
    {
        std::unique_ptr<BrainCloud::URLLoader> loader;
        std::string requestedUrl;
        std::string title;
        std::string author;
        std::string authorUrl;
        std::string siteName;
        std::string description;
        std::string imageUrl;
        std::string error;
        unsigned int texture = 0;
        std::vector<unsigned int> frames;
        std::vector<int> frameDelays;
        std::future<DecodedImage> decodeFuture;
        std::shared_ptr<DecodedImage> pendingImage;
        int uploadedFrames = 0;
        double animationStarted = 0.0;
        int width = 0;
        int height = 0;
        bool started = false;
        bool complete = false;
        bool imageExpected = false;
        bool jsonExpected = false;
        bool mediaDetected = false;
    };

    std::map<std::string, Preview> previews;
    ImFont* boldFont = nullptr;
    ImFont* providerFont = nullptr;
    ImFont* titleFont = nullptr;

    size_t activeLoads()
    {
        size_t count = 0;
        for (const auto& entry : previews)
            if (entry.second.loader || entry.second.decodeFuture.valid() || entry.second.pendingImage) ++count;
        return count;
    }

    std::string lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool hasImageExtension(const std::string& url)
    {
        std::string path = lower(url.substr(0, url.find_first_of("?#")));
        const char* extensions[] = { ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp" };
        for (const char* extension : extensions)
            if (path.size() >= strlen(extension) && path.compare(path.size() - strlen(extension), strlen(extension), extension) == 0)
                return true;
        return false;
    }

    bool hasImageSignature(const std::string& bytes)
    {
        if (bytes.size() < 12) return false;
        return bytes.compare(0, 3, "\xFF\xD8\xFF") == 0 ||
            bytes.compare(0, 8, "\x89PNG\r\n\x1A\n") == 0 ||
            bytes.compare(0, 6, "GIF87a") == 0 || bytes.compare(0, 6, "GIF89a") == 0 ||
            (bytes.compare(0, 4, "RIFF") == 0 && bytes.compare(8, 4, "WEBP") == 0);
    }

    std::string youtubeId(const std::string& url)
    {
        std::string value = url;
        size_t pos = value.find("youtu.be/");
        if (pos != std::string::npos)
            value = value.substr(pos + 9);
        else
        {
            pos = value.find("youtube.com/watch");
            if (pos == std::string::npos) return std::string();
            pos = value.find("v=", pos);
            if (pos == std::string::npos) return std::string();
            value = value.substr(pos + 2);
        }
        size_t end = value.find_first_of("&#?/ ");
        return value.substr(0, end);
    }

    std::string hostName(const std::string& url)
    {
        size_t start = url.find("://");
        start = start == std::string::npos ? 0 : start + 3;
        size_t end = url.find_first_of("/:?#", start);
        return url.substr(start, end - start);
    }

    std::string percentEncode(const std::string& value)
    {
        static const char hex[] = "0123456789ABCDEF";
        std::string encoded;
        for (unsigned char character : value)
        {
            if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') || character == '-' || character == '_' ||
                character == '.' || character == '~')
                encoded.push_back(static_cast<char>(character));
            else
            {
                encoded.push_back('%');
                encoded.push_back(hex[character >> 4]);
                encoded.push_back(hex[character & 0x0F]);
            }
        }
        return encoded;
    }

    void appendUtf8(std::string& output, unsigned int codepoint)
    {
        if (codepoint <= 0x7F) output.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7FF)
        {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else if (codepoint <= 0xFFFF)
        {
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else if (codepoint <= 0x10FFFF)
        {
            output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    std::string decodeHtml(const std::string& input)
    {
        static const std::map<std::string, unsigned int> named = {
            { "amp", '&' }, { "apos", '\'' }, { "gt", '>' }, { "hellip", 0x2026 },
            { "lt", '<' }, { "mdash", 0x2014 }, { "nbsp", ' ' }, { "ndash", 0x2013 },
            { "quot", '"' }
        };
        std::string decoded;
        for (size_t cursor = 0; cursor < input.size();)
        {
            if (input[cursor] != '&')
            {
                decoded.push_back(input[cursor++]);
                continue;
            }
            size_t semicolon = input.find(';', cursor + 1);
            if (semicolon == std::string::npos || semicolon - cursor > 12)
            {
                decoded.push_back(input[cursor++]);
                continue;
            }
            std::string entity = input.substr(cursor + 1, semicolon - cursor - 1);
            unsigned int codepoint = 0;
            bool recognized = false;
            if (!entity.empty() && entity[0] == '#')
            {
                size_t digit = 1;
                int base = 10;
                if (digit < entity.size() && (entity[digit] == 'x' || entity[digit] == 'X'))
                {
                    base = 16;
                    ++digit;
                }
                recognized = digit < entity.size();
                for (; recognized && digit < entity.size(); ++digit)
                {
                    char character = entity[digit];
                    int number = character >= '0' && character <= '9' ? character - '0' :
                        character >= 'a' && character <= 'f' ? character - 'a' + 10 :
                        character >= 'A' && character <= 'F' ? character - 'A' + 10 : -1;
                    if (number < 0 || number >= base) recognized = false;
                    else codepoint = codepoint * base + number;
                }
            }
            else
            {
                auto found = named.find(lower(entity));
                if (found != named.end())
                {
                    codepoint = found->second;
                    recognized = true;
                }
            }
            if (recognized)
            {
                appendUtf8(decoded, codepoint);
                cursor = semicolon + 1;
            }
            else decoded.push_back(input[cursor++]);
        }

        std::string normalized;
        bool pendingSpace = false;
        for (unsigned char character : decoded)
        {
            if (std::isspace(character)) pendingSpace = !normalized.empty();
            else
            {
                if (pendingSpace) normalized.push_back(' ');
                normalized.push_back(static_cast<char>(character));
                pendingSpace = false;
            }
        }
        return normalized;
    }

    std::string htmlTitle(const std::string& html)
    {
        std::string folded = lower(html);
        size_t start = folded.find("<title");
        if (start == std::string::npos) return std::string();
        start = folded.find('>', start);
        size_t end = folded.find("</title>", start);
        if (start == std::string::npos || end == std::string::npos) return std::string();
        std::string title = decodeHtml(html.substr(start + 1, end - start - 1));
        if (title.size() > 100) title = title.substr(0, 97) + "...";
        return title;
    }

#ifndef BCCHAT_UWP
    void resizeRgba(const unsigned char* source, int sourceWidth, int sourceHeight,
        int targetWidth, int targetHeight, std::vector<unsigned char>& target)
    {
        target.resize(static_cast<size_t>(targetWidth) * targetHeight * 4);
        for (int y = 0; y < targetHeight; ++y)
        {
            int sourceY = y * sourceHeight / targetHeight;
            for (int x = 0; x < targetWidth; ++x)
            {
                int sourceX = x * sourceWidth / targetWidth;
                const unsigned char* sourcePixel = source + (static_cast<size_t>(sourceY) * sourceWidth + sourceX) * 4;
                unsigned char* targetPixel = target.data() + (static_cast<size_t>(y) * targetWidth + x) * 4;
                targetPixel[0] = sourcePixel[0];
                targetPixel[1] = sourcePixel[1];
                targetPixel[2] = sourcePixel[2];
                targetPixel[3] = sourcePixel[3];
            }
        }
    }

    DecodedImage decodeImage(const std::shared_ptr<std::string>& bytes)
    {
        DecodedImage decoded;
        int channels = 0;
        int* delays = nullptr;
        unsigned char* pixels = nullptr;
        bool gif = bytes->size() >= 6 &&
            (bytes->compare(0, 6, "GIF87a") == 0 || bytes->compare(0, 6, "GIF89a") == 0);
        if (gif)
            pixels = stbi_load_gif_from_memory(reinterpret_cast<const unsigned char*>(bytes->data()),
                static_cast<int>(bytes->size()), &delays, &decoded.sourceWidth, &decoded.sourceHeight,
                &decoded.frameCount, &channels, 4);
        else
        {
            pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes->data()),
                static_cast<int>(bytes->size()), &decoded.sourceWidth, &decoded.sourceHeight, &channels, 4);
            decoded.frameCount = pixels ? 1 : 0;
        }
        if (!pixels)
        {
            decoded.error = "Unsupported or invalid image";
            return decoded;
        }

        size_t sourceFrameBytes = static_cast<size_t>(decoded.sourceWidth) * decoded.sourceHeight * 4;
        if (sourceFrameBytes > PREVIEW_MAX_DECODED_BYTES / static_cast<size_t>(decoded.frameCount))
        {
            decoded.error = "Decoded image is too large";
            stbi_image_free(pixels);
            if (delays) STBI_FREE(delays);
            return decoded;
        }

        float scale = std::min(1.0f, std::min(
            PREVIEW_MAX_WIDTH / decoded.sourceWidth, PREVIEW_MAX_HEIGHT / decoded.sourceHeight));
        decoded.textureWidth = std::max(1, static_cast<int>(decoded.sourceWidth * scale));
        decoded.textureHeight = std::max(1, static_cast<int>(decoded.sourceHeight * scale));
        size_t targetFrameBytes = static_cast<size_t>(decoded.textureWidth) * decoded.textureHeight * 4;
        decoded.pixels.resize(targetFrameBytes * decoded.frameCount);
        decoded.delays.resize(decoded.frameCount, 100);

        std::vector<unsigned char> resized;
        for (int frame = 0; frame < decoded.frameCount; ++frame)
        {
            const unsigned char* sourceFrame = pixels + sourceFrameBytes * frame;
            unsigned char* targetFrame = decoded.pixels.data() + targetFrameBytes * frame;
            if (decoded.textureWidth == decoded.sourceWidth && decoded.textureHeight == decoded.sourceHeight)
                std::copy(sourceFrame, sourceFrame + sourceFrameBytes, targetFrame);
            else
            {
                resizeRgba(sourceFrame, decoded.sourceWidth, decoded.sourceHeight,
                    decoded.textureWidth, decoded.textureHeight, resized);
                std::copy(resized.begin(), resized.end(), targetFrame);
            }
            if (delays) decoded.delays[frame] = std::max(20, delays[frame]);
        }
        stbi_image_free(pixels);
        if (delays) STBI_FREE(delays);
        return decoded;
    }
#endif

    std::string htmlAttribute(const std::string& tag, const std::string& attribute)
    {
        std::regex expression("(?:^|\\s)" + attribute + "\\s*=\\s*[\"']([^\"']*)[\"']",
            std::regex_constants::icase);
        std::smatch match;
        return std::regex_search(tag, match, expression) ? match[1].str() : std::string();
    }

    std::string resolveUrl(const std::string& pageUrl, const std::string& value)
    {
        if (value.find("://") != std::string::npos) return value;
        size_t schemeEnd = pageUrl.find("://");
        if (schemeEnd == std::string::npos) return value;
        if (value.compare(0, 2, "//") == 0) return pageUrl.substr(0, schemeEnd) + ":" + value;
        size_t originEnd = pageUrl.find('/', schemeEnd + 3);
        std::string origin = pageUrl.substr(0, originEnd);
        if (!value.empty() && value[0] == '/') return origin + value;
        size_t pathEnd = pageUrl.find_last_of('/');
        return pageUrl.substr(0, pathEnd == std::string::npos ? pageUrl.size() : pathEnd + 1) + value;
    }

    void parseHtmlMetadata(Preview& preview, const std::string& html)
    {
        std::string documentTitle = htmlTitle(html);
        std::regex metaTag("<meta\\b[^>]*>", std::regex_constants::icase);
        for (std::sregex_iterator iterator(html.begin(), html.end(), metaTag), end; iterator != end; ++iterator)
        {
            std::string tag = iterator->str();
            std::string key = lower(htmlAttribute(tag, "property"));
            if (key.empty()) key = lower(htmlAttribute(tag, "name"));
            if (key.empty()) key = lower(htmlAttribute(tag, "itemprop"));
            std::string content = decodeHtml(htmlAttribute(tag, "content"));
            if (content.empty()) continue;
            if (key == "og:title") preview.title = content;
            else if (key == "og:description") preview.description = content;
            else if (key == "og:image") preview.imageUrl = resolveUrl(preview.requestedUrl, content);
            else if (key == "og:site_name") preview.siteName = content;
            else if (key == "twitter:title" && preview.title.empty()) preview.title = content;
            else if (key == "twitter:description" && preview.description.empty()) preview.description = content;
            else if (key == "twitter:image" && preview.imageUrl.empty()) preview.imageUrl = resolveUrl(preview.requestedUrl, content);
            else if (key == "description" && preview.description.empty()) preview.description = content;
        }
        if (preview.title.empty()) preview.title = documentTitle;

        std::string foldedTitle = lower(preview.title);
        if (foldedTitle == "loading" || foldedTitle == "loading..." ||
            foldedTitle == "untitled" || foldedTitle == "untitled page")
        {
            // Some client-rendered apps leave a placeholder document title in
            // their initial HTML. Promote meaningful metadata instead.
            preview.title = preview.description;
            preview.description.clear();
        }

        // Google's homepage omits its usual description for some crawler
        // locations/user agents, while rich-preview services retain this
        // canonical summary. Keep the card useful in that response variant.
        std::string pageHost = lower(hostName(preview.requestedUrl));
        if (preview.description.empty() && (pageHost == "google.com" || pageHost == "www.google.com"))
            preview.description = "Search the world's information, including webpages, images, videos and more. "
                "Google has many special features to help you find exactly what you're looking for.";
        if (preview.description.size() > 300)
        {
            size_t cut = preview.description.rfind(' ', 297);
            preview.description = preview.description.substr(0, cut == std::string::npos ? 297 : cut) + "...";
        }
    }

    void beginLoad(Preview& preview, const std::string& url, bool imageExpected, bool jsonExpected = false)
    {
        preview.started = true;
        preview.imageExpected = imageExpected;
        preview.jsonExpected = jsonExpected;
        preview.requestedUrl = url;
        preview.loader.reset(BrainCloud::URLLoader::create());
        if (!preview.loader)
        {
            preview.complete = true;
            preview.error = "Preview unavailable";
            return;
        }
        URLRequest request(url);
        request.setMethod("GET");
        request.setUserAgent("BCChat/1.0");
        preview.loader->setTimeout(30000);
        preview.loader->load(request);
    }

    void finishLoad(Preview& preview)
    {
#ifndef BCCHAT_UWP
        if (preview.decodeFuture.valid())
        {
            if (preview.decodeFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                return;
            DecodedImage decoded = preview.decodeFuture.get();
            if (!decoded.error.empty())
            {
                preview.error = decoded.error;
                preview.complete = true;
                return;
            }
            preview.pendingImage = std::make_shared<DecodedImage>(std::move(decoded));
            preview.width = preview.pendingImage->sourceWidth;
            preview.height = preview.pendingImage->sourceHeight;
            preview.frames.resize(preview.pendingImage->frameCount);
            preview.frameDelays = preview.pendingImage->delays;
            glGenTextures(static_cast<GLsizei>(preview.frames.size()), preview.frames.data());
        }

        if (preview.pendingImage)
        {
            const int framesPerUiFrame = 4;
            size_t frameBytes = static_cast<size_t>(preview.pendingImage->textureWidth) *
                preview.pendingImage->textureHeight * 4;
            int uploadEnd = std::min(preview.pendingImage->frameCount,
                preview.uploadedFrames + framesPerUiFrame);
            for (; preview.uploadedFrames < uploadEnd; ++preview.uploadedFrames)
            {
                const unsigned char* framePixels = preview.pendingImage->pixels.data() +
                    frameBytes * preview.uploadedFrames;
                glBindTexture(GL_TEXTURE_2D, preview.frames[preview.uploadedFrames]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, preview.pendingImage->textureWidth,
                    preview.pendingImage->textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, framePixels);
            }
            if (!preview.frames.empty()) preview.texture = preview.frames.front();
            if (preview.uploadedFrames == preview.pendingImage->frameCount)
            {
                preview.animationStarted = ImGui::GetTime();
                preview.pendingImage.reset();
                preview.complete = true;
            }
            return;
        }
#endif

        if (!preview.loader || !preview.loader->isDone()) return;
        preview.complete = true;
        URLResponse& response = preview.loader->getResponse();
        if (response.getStatusCode() < 200 || response.getStatusCode() >= 300)
        {
            preview.error = "Preview failed (HTTP " + std::to_string(response.getStatusCode()) + ")";
            preview.loader.reset();
            return;
        }

        const std::string& bytes = response.getData();
        if (bytes.size() > PREVIEW_MAX_DOWNLOAD_BYTES)
        {
            preview.error = "Preview is too large";
            preview.loader.reset();
            return;
        }
        if (preview.jsonExpected)
        {
            Json::Value metadata;
            Json::Reader reader;
            if (reader.parse(bytes, metadata))
            {
                preview.title = metadata.get("title", "YouTube video").asString();
                preview.author = metadata.get("author_name", "").asString();
                preview.authorUrl = metadata.get("author_url", "").asString();
            }
            else preview.error = "Video metadata unavailable";
        }
        else
        {
#ifndef BCCHAT_UWP
        if (preview.imageExpected || lower(response.getContentType()).find("image/") == 0 || hasImageSignature(bytes))
        {
            preview.mediaDetected = true;
            std::shared_ptr<std::string> imageBytes = std::make_shared<std::string>(bytes);
            preview.complete = false;
            preview.decodeFuture = std::async(std::launch::async, [imageBytes]()
            {
                return decodeImage(imageBytes);
            });
            preview.loader.reset();
            return;
        }
        else
#endif
        {
            parseHtmlMetadata(preview, bytes);
        }
        }
        preview.loader.reset();
    }

    void drawLink(const std::string& label, const std::string& url, ImFont* font = nullptr,
        const ImVec4& color = ImVec4(0.25f, 0.65f, 1.0f, 1.0f))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (font) ImGui::PushFont(font);
        ImGui::TextWrapped("%s", label.c_str());
        if (font) ImGui::PopFont();
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("%s", url.c_str());
        }
#ifndef BCCHAT_UWP
        if (ImGui::IsItemClicked()) SDL_OpenURL(url.c_str());
#endif
    }
}

std::vector<std::string> mediaPreview_findUrls(const std::string& text)
{
    std::vector<std::string> urls;
    size_t cursor = 0;
    while (cursor < text.size())
    {
        size_t http = text.find("http://", cursor);
        size_t https = text.find("https://", cursor);
        size_t start = std::min(http, https);
        if (start == std::string::npos) start = std::max(http, https);
        if (start == std::string::npos) break;
        size_t end = text.find_first_of(" \t\r\n<>\"", start);
        std::string url = text.substr(start, end - start);
        while (!url.empty() && (url.back() == '.' || url.back() == ',' || url.back() == '!' || url.back() == ')' || url.back() == ']'))
            url.pop_back();
        if (!url.empty()) urls.push_back(url);
        cursor = end == std::string::npos ? text.size() : end;
    }
    return urls;
}

bool mediaPreview_isDirectMediaUrl(const std::string& url)
{
    if (hasImageExtension(url)) return true;
    auto found = previews.find(url);
    return found != previews.end() && found->second.mediaDetected;
}

void mediaPreview_draw(const std::string& originalUrl)
{
    std::string id = youtubeId(originalUrl);
    std::string previewUrl = id.empty() ? originalUrl : "https://img.youtube.com/vi/" + id + "/hqdefault.jpg";
    bool image = hasImageExtension(previewUrl) || !id.empty();
    Preview& preview = previews[originalUrl];
    // Avoid a large history page starting dozens of HTTP requests at once.
    if (!preview.started && activeLoads() < 4) beginLoad(preview, previewUrl, image);
    finishLoad(preview);

    Preview* videoMetadata = nullptr;
    if (!id.empty())
    {
        Preview& metadata = previews["youtube metadata##" + originalUrl];
        std::string metadataUrl = "https://www.youtube.com/oembed?format=json&url=" + percentEncode(originalUrl);
        if (!metadata.started && activeLoads() < 4) beginLoad(metadata, metadataUrl, false, true);
        finishLoad(metadata);
        videoMetadata = &metadata;
    }

    Preview* pageImage = nullptr;
    if (id.empty() && preview.complete && !preview.imageUrl.empty())
    {
        Preview& imageMetadata = previews["page image##" + originalUrl];
        if (!imageMetadata.started && activeLoads() < 4) beginLoad(imageMetadata, preview.imageUrl, true);
        finishLoad(imageMetadata);
        pageImage = &imageMetadata;
    }

    Preview* displayedImage = !id.empty() ? &preview :
        (pageImage ? pageImage : (preview.texture ? &preview : nullptr));

    float imageWidth = 0.0f;
    float imageHeight = 0.0f;
#ifndef BCCHAT_UWP
    if (displayedImage && displayedImage->texture)
    {
        float scale = std::min(1.0f, std::min(PREVIEW_MAX_WIDTH / displayedImage->width, PREVIEW_MAX_HEIGHT / displayedImage->height));
        imageWidth = displayedImage->width * scale;
        imageHeight = displayedImage->height * scale;
    }

    // A URL whose response is itself an image/GIF is media, not a web page.
    // Draw it directly in the feed; Open Graph images remain inside cards.
    bool directMedia = id.empty() && displayedImage == &preview && preview.texture &&
        preview.title.empty() && preview.siteName.empty() && preview.description.empty();
    if (directMedia)
    {
        if (preview.complete && preview.frames.size() > 1)
        {
            int elapsed = static_cast<int>((ImGui::GetTime() - preview.animationStarted) * 1000.0);
            int cycle = 0;
            for (int delay : preview.frameDelays) cycle += delay;
            elapsed %= cycle;
            size_t frame = 0;
            while (frame + 1 < preview.frames.size() && elapsed >= preview.frameDelays[frame])
            {
                elapsed -= preview.frameDelays[frame];
                ++frame;
            }
            preview.texture = preview.frames[frame];
        }
        ImGui::Indent(8.0f);
        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(preview.texture)),
            ImVec2(imageWidth, imageHeight));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("Open image");
        }
        if (ImGui::IsItemClicked()) SDL_OpenURL(originalUrl.c_str());
        ImGui::Unindent(8.0f);
        return;
    }
#endif
    float cardWidth = std::min(400.0f, ImGui::GetContentRegionAvail().x - 12.0f);
    cardWidth = std::max(220.0f, cardWidth);
    std::string titleText = videoMetadata && !videoMetadata->title.empty() ? videoMetadata->title :
        (!id.empty() ? "YouTube video" : preview.title);
    std::string authorText = videoMetadata ? videoMetadata->author : std::string();
    std::string providerText = !id.empty() ? "YouTube" : preview.siteName;
    std::string descriptionText = id.empty() ? preview.description : std::string();
    if (titleText.empty() && preview.complete && preview.error.empty()) titleText = "Web page";
    const float innerWidth = cardWidth - 20.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    float cardHeight = 16.0f; // top and bottom padding
    if (!providerText.empty())
    {
        if (providerFont) ImGui::PushFont(providerFont);
        cardHeight += ImGui::CalcTextSize(providerText.c_str(), nullptr, false, innerWidth).y;
        if (providerFont) ImGui::PopFont();
    }
    if (boldFont) ImGui::PushFont(boldFont);
    if (!authorText.empty()) cardHeight += spacing + ImGui::CalcTextSize(authorText.c_str(), nullptr, false, innerWidth).y;
    if (boldFont) ImGui::PopFont();
    if (titleFont) ImGui::PushFont(titleFont);
    if (!titleText.empty()) cardHeight += spacing + ImGui::CalcTextSize(titleText.c_str(), nullptr, false, innerWidth).y;
    if (titleFont) ImGui::PopFont();
    if (boldFont) ImGui::PushFont(boldFont);
    if (!descriptionText.empty()) cardHeight += spacing + ImGui::CalcTextSize(descriptionText.c_str(), nullptr, false, innerWidth).y;
    if (!preview.complete || !preview.error.empty()) cardHeight += spacing + ImGui::GetTextLineHeight();
    if (imageHeight > 0.0f) cardHeight += spacing + imageHeight;
    if (boldFont) ImGui::PopFont();

    ImGui::Indent(8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::BeginChild(("Link card##" + originalUrl).c_str(), ImVec2(cardWidth, cardHeight), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysUseWindowPadding);
    if (id.empty() && !providerText.empty())
        drawLink(providerText, originalUrl, providerFont, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    else if (!id.empty()) drawLink("YouTube", "https://www.youtube.com/", providerFont, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (!authorText.empty())
        drawLink(authorText, videoMetadata->authorUrl.empty() ? originalUrl : videoMetadata->authorUrl, boldFont,
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (!titleText.empty())
        drawLink(titleText, originalUrl, titleFont ? titleFont : boldFont);
    if (!descriptionText.empty())
    {
        if (boldFont) ImGui::PushFont(boldFont);
        ImGui::TextWrapped("%s", descriptionText.c_str());
        if (boldFont) ImGui::PopFont();
    }
#ifndef BCCHAT_UWP
    if (displayedImage && displayedImage->texture)
    {
        if (displayedImage->complete && displayedImage->frames.size() > 1)
        {
            int elapsed = static_cast<int>((ImGui::GetTime() - displayedImage->animationStarted) * 1000.0);
            int cycle = 0;
            for (int delay : displayedImage->frameDelays) cycle += delay;
            elapsed %= cycle;
            size_t frame = 0;
            while (frame + 1 < displayedImage->frames.size() && elapsed >= displayedImage->frameDelays[frame])
            {
                elapsed -= displayedImage->frameDelays[frame];
                ++frame;
            }
            displayedImage->texture = displayedImage->frames[frame];
        }
        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(displayedImage->texture)),
            ImVec2(imageWidth, imageHeight));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("%s", id.empty() ? "Open web page" : "Open video on YouTube");
        }
        if (ImGui::IsItemClicked()) SDL_OpenURL(originalUrl.c_str());
    }
#endif
    if (!preview.complete) ImGui::TextDisabled("Loading preview...");
    else if (!preview.error.empty()) ImGui::TextDisabled("%s", preview.error.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::Unindent(8.0f);
}

void mediaPreview_shutdown()
{
#ifndef BCCHAT_UWP
    for (auto& entry : previews)
        if (!entry.second.frames.empty())
            glDeleteTextures(static_cast<GLsizei>(entry.second.frames.size()), entry.second.frames.data());
#endif
    previews.clear();
}

void mediaPreview_initializeFonts()
{
#if defined(_WIN32) && !defined(BCCHAT_UWP)
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    static const ImWchar symbolRanges[] = {
        0x2000, 0x2BFF,
        0
    };
    static const ImWchar emojiRanges[] = {
#ifdef IMGUI_USE_WCHAR32
        0x1F000, 0x1FAFF,
#endif
        0
    };
    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisym.ttf", 16.0f, &config, symbolRanges);
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguiemj.ttf", 16.0f, &config, emojiRanges);

    const char* basePath = SDL_GetBasePath();
    if (basePath)
    {
        std::string fontPath = std::string(basePath) + "Roboto-Medium.ttf";
        boldFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
        providerFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 13.0f);
        titleFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 17.0f);
    }
#endif
}
