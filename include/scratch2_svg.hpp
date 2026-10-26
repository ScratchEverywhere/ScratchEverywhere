#pragma once
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

// Scratch 2's SVG exporter writes text coordinates that its own renderer ignores.
// Match the text conversion in scratchfoundation/scratch-svg-renderer:
// src/load-svg-string.js (transformText). Apply only to original SB2 assets.
namespace Scratch2Svg {
inline bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

struct Attribute {
    size_t begin = std::string::npos, end = 0, valueBegin = 0, valueEnd = 0;
};

// Read attributes without changing unrelated XML, quoted values or entities.
inline Attribute attribute(const std::string &tag, std::string_view name) {
    size_t p = tag.find_first_of(" \t\r\n>");
    while (p < tag.size()) {
        while (p < tag.size() && space(tag[p])) ++p;
        if (p == tag.size() || tag[p] == '>' || tag[p] == '/') break;
        size_t begin = p;
        while (p < tag.size() && !space(tag[p]) && tag[p] != '=' && tag[p] != '>') ++p;
        const size_t nameEnd = p;
        while (p < tag.size() && space(tag[p])) ++p;
        if (p == tag.size() || tag[p++] != '=') return {};
        while (p < tag.size() && space(tag[p])) ++p;
        if (p == tag.size() || (tag[p] != '\'' && tag[p] != '"')) return {};
        const char quote = tag[p++];
        const size_t valueBegin = p;
        p = tag.find(quote, p);
        if (p == std::string::npos) return {};
        const size_t valueEnd = p++;
        if (std::string_view(tag).substr(begin, nameEnd - begin) == name)
            return {begin, p, valueBegin, valueEnd};
    }
    return {};
}

inline std::string get(const std::string &tag, std::string_view name) {
    auto a = attribute(tag, name);
    return a.begin == std::string::npos ? "" : tag.substr(a.valueBegin, a.valueEnd - a.valueBegin);
}

inline void set(std::string &tag, std::string_view name, const std::string &value) {
    auto a = attribute(tag, name);
    if (a.begin != std::string::npos) tag.replace(a.begin, a.end - a.begin, std::string(name) + "=\"" + value + "\"");
    else tag.insert(tag.size() - 1, " " + std::string(name) + "=\"" + value + "\"");
}

inline void remove(std::string &tag, std::string_view name) {
    auto a = attribute(tag, name);
    if (a.begin != std::string::npos) tag.erase(a.begin, a.end - a.begin);
}

inline std::string normalizeText(const std::string_view svg) {
    std::string result;
    result.reserve(svg.size());
    size_t p = 0;
    while (p < svg.size()) {
        const size_t begin = svg.find('<', p);
        if (begin == std::string_view::npos) { result.append(svg.substr(p)); break; }
        result.append(svg.substr(p, begin - p));
        // Skip comments/CDATA wholesale: text-looking markup there is not an element.
        std::string_view terminator;
        if (svg.substr(begin, 4) == "<!--") terminator = "-->";
        else if (svg.substr(begin, 9) == "<![CDATA[") terminator = "]]>";
        if (!terminator.empty()) {
            auto end = svg.find(terminator, begin + 4);
            p = end == std::string_view::npos ? svg.size() : end + terminator.size();
            result.append(svg.substr(begin, p - begin));
            continue;
        }
        size_t end = begin + 1;
        char quote = 0;
        for (; end < svg.size(); ++end) {
            char c = svg[end];
            if (quote) { if (c == quote) quote = 0; }
            else if (c == '\'' || c == '"') quote = c;
            else if (c == '>') break;
        }
        if (end == svg.size()) { result.append(svg.substr(begin)); break; }
        const bool text = svg.substr(begin, 5) == "<text" && (space(svg[begin + 5]) || svg[begin + 5] == '>');
        const size_t close = text ? svg.find("</text>", end + 1) : std::string_view::npos;
        if (!text || close == std::string_view::npos) {
            result.append(svg.substr(begin, end + 1 - begin)); p = end + 1; continue;
        }
        std::string tag(svg.substr(begin, end + 1 - begin));
        auto body = svg.substr(end + 1, close - end - 1);
        const auto family = get(tag, "font-family");
        const auto fontSize = get(tag, "font-size");
        float size = fontSize.empty() ? 18.0f : std::strtof(fontSize.c_str(), nullptr);
        if (!std::isfinite(size) || size <= 0) size = 18.0f;
        const char *spacing = "1.2";
        float offset = 0;
        if (family == "Handwriting") { spacing = "2"; offset = -11; }
        else if (family == "Scratch") { spacing = "0.89"; offset = -3; }
        else if (family == "Curly") { spacing = "1.38"; offset = -6; }
        else if (family == "Marker") { spacing = "1.45"; offset = -6; }
        else if (family == "Sans Serif") { spacing = "1.13"; offset = -3; }
        else if (family == "Serif") { spacing = "1.25"; offset = -4; }
        remove(tag, "x");
        remove(tag, "y");
        set(tag, "alignment-baseline", "text-before-edge");
        set(tag, "xml:space", "preserve");
        if (fontSize.empty()) set(tag, "font-size", "18");
        set(tag, "transform", get(tag, "transform") + " translate(2 " + std::to_string(offset * size / 22.0f) + ")");
        result += tag;
        if (!body.empty() && body.find('<') == std::string_view::npos) {
            // XML parsers normalize both CR and CRLF before exposing textContent.
            // Original Scratch 2 exports (including Asteroids) use lone CRs.
            std::string lines;
            lines.reserve(body.size());
            for (size_t i = 0; i < body.size(); ++i) {
                if (body[i] == '\r') {
                    lines += '\n';
                    if (i + 1 < body.size() && body[i + 1] == '\n') ++i;
                } else lines += body[i];
            }
            body = lines;
            size_t line = 0;
            do {
                auto next = body.find('\n', line);
                auto content = body.substr(line, next == std::string_view::npos ? next : next - line);
                result += "<tspan x=\"0\" style=\"white-space: pre\" dy=\"";
                result += spacing;
                result += "em\">";
                result.append(content.empty() ? std::string_view(" ") : content);
                result += "</tspan>";
                if (next == std::string_view::npos) break;
                line = next + 1;
            } while (line <= body.size());
        } else result.append(body);
        result += "</text>";
        p = close + 7;
    }
    return result;
}
} // namespace Scratch2Svg
