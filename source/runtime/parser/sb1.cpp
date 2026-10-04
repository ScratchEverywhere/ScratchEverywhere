#include "parser.hpp"
#include "project_loader.hpp"
#include "sb1_squeak.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <istream>
#include <log.hpp>
#include <memory>
#include <unordered_map>

namespace {

constexpr uint32_t defaultOneBitColormap[2] = {0xFFFFFFFF, 0xFF000000};
std::vector<uint32_t> greyscaleColormap(int depth) {
    size_t n = size_t(1) << depth;
    std::vector<uint32_t> map(n);
    for (size_t i = 0; i < n; i++) {
        uint32_t v = static_cast<uint32_t>(i * 255 / (n > 1 ? n - 1 : 1));
        map[i] = 0xFF000000 | (v << 16) | (v << 8) | v;
    }
    return map;
}

uint32_t readVariableInt(const std::vector<uint8_t> &buf, size_t &pos) {
    uint8_t first = pos < buf.size() ? buf[pos] : 0;
    if (first <= 223) {
        pos += 1;
        return first;
    }
    if (first <= 254) {
        uint8_t second = pos + 1 < buf.size() ? buf[pos + 1] : 0;
        pos += 2;
        return ((first - 224) * 256) + second;
    }
    uint32_t v = 0;
    for (int i = 1; i <= 4; i++)
        v = (v << 8) | (pos + i < buf.size() ? buf[pos + i] : 0);
    pos += 5;
    return v;
}

std::vector<uint32_t> decodeRlePixels(const std::vector<uint8_t> &bytes, bool withAlpha) {
    size_t pos = 0;
    uint32_t pixelsOut = readVariableInt(bytes, pos);
    std::vector<uint32_t> result(pixelsOut, 0);

    const auto readU8 = [&]() -> uint8_t { return pos < bytes.size() ? bytes[pos++] : 0; };
    const auto readU32be = [&]() -> uint32_t {
        uint32_t v = readU8();
        v = (v << 8) | readU8();
        v = (v << 8) | readU8();
        v = (v << 8) | readU8();
        return v;
    };

    uint32_t i = 0;
    while (i < pixelsOut && pos < bytes.size()) {
        uint32_t runLengthAndCode = readVariableInt(bytes, pos);
        uint32_t runLength = runLengthAndCode >> 2;
        uint32_t code = runLengthAndCode & 0b11;

        switch (code) {
        case 0:
            i += runLength;
            break;
        case 1: {
            uint32_t w = readU8();
            w = (w << 24) | (w << 16) | (w << 8) | w;
            if (withAlpha && w != 0) w |= 0xff000000;
            for (uint32_t j = 0; j < runLength && i < pixelsOut; j++)
                result[i++] = w;
            break;
        }
        case 2: {
            uint32_t w = readU32be();
            if (withAlpha && w != 0) w |= 0xff000000;
            for (uint32_t j = 0; j < runLength && i < pixelsOut; j++)
                result[i++] = w;
            break;
        }
        case 3:
            for (uint32_t j = 0; j < runLength && i < pixelsOut; j++) {
                uint32_t w = readU32be();
                if (withAlpha && w != 0) w |= 0xff000000;
                result[i++] = w;
            }
            break;
        }
    }
    return result;
}

std::vector<uint32_t> unpackPixels(const std::vector<uint32_t> &words, int width, int height, int depth, const std::vector<uint32_t> &colormap) {
    std::vector<uint32_t> result(static_cast<size_t>(width) * height, 0);
    uint32_t mask = (1u << depth) - 1;
    int pixelsPerWord = 32 / depth;
    size_t dst = 0, src = 0;
    for (int y = 0; y < height; y++) {
        int shift = -1;
        uint32_t word = 0;
        for (int x = 0; x < width; x++) {
            if (shift < 0) {
                shift = depth * (pixelsPerWord - 1);
                word = src < words.size() ? words[src++] : 0;
            }
            uint32_t index = (word >> shift) & mask;
            result[dst++] = index < colormap.size() ? colormap[index] : 0;
            shift -= depth;
        }
    }
    return result;
}

std::vector<uint32_t> raster16To32(const std::vector<uint32_t> &words, int width, int height) {
    std::vector<uint32_t> result(static_cast<size_t>(width) * height, 0);
    size_t dst = 0, src = 0;
    for (int y = 0; y < height; y++) {
        int shift = -1;
        uint32_t word = 0;
        for (int x = 0; x < width; x++) {
            if (shift < 0) {
                shift = 16;
                word = src < words.size() ? words[src++] : 0;
            }
            uint32_t pix = (word >> shift) & 0xffff;
            if (pix != 0) {
                uint32_t red = (pix >> 7) & 0b11111000;
                uint32_t green = (pix >> 2) & 0b11111000;
                uint32_t blue = (pix << 3) & 0b11111000;
                pix = 0xff000000 | (red << 16) | (green << 8) | blue;
            }
            result[dst++] = pix;
            shift -= 16;
        }
    }
    return result;
}

std::vector<uint32_t> decodeSqueakImage(const SqFieldPtr &form) {
    if (!form) return {};
    int width = static_cast<int>(form->field(0) ? form->field(0)->numberValue : 0);
    int height = static_cast<int>(form->field(1) ? form->field(1)->numberValue : 0);
    int depth = static_cast<int>(form->field(2) ? form->field(2)->numberValue : 0);
    SqFieldPtr bytesField = form->field(4);
    SqFieldPtr colormapField = form->field(5);
    if (width <= 0 || height <= 0 || !bytesField) return {};

    bool withAlpha = depth == 32;
    std::vector<uint32_t> words;
    if (bytesField->kind == SqField::Kind::Bitmap) {
        words = bytesField->bitmapValue;
        if (withAlpha) {
            for (auto &w : words)
                if (w != 0) w |= 0xff000000;
        }
    } else if (bytesField->kind == SqField::Kind::Bytes) {
        words = decodeRlePixels(bytesField->bytesValue, withAlpha);
    } else {
        return {};
    }

    if (depth <= 8) {
        std::vector<uint32_t> colormap;
        if (colormapField && colormapField->kind == SqField::Kind::Array) {
            colormap.reserve(colormapField->items.size());
            for (const auto &c : colormapField->items)
                colormap.push_back(c ? static_cast<uint32_t>(c->numberValue) : 0);
        } else if (depth == 1) {
            colormap.assign(defaultOneBitColormap, defaultOneBitColormap + 2);
        } else {
            colormap = greyscaleColormap(depth);
        }
        return unpackPixels(words, width, height, depth, colormap);
    }
    if (depth == 16) return raster16To32(words, width, height);
    if (depth == 32) {
        words.resize(static_cast<size_t>(width) * height, 0);
        return words;
    }
    return {};
}

constexpr int squeakStepSizeTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
    45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209,
    230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
    963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749,
    3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767};

const std::vector<int> &squeakIndexTable(int bitsPerSample) {
    static const std::vector<int> t2 = {-1, 2, -1, 2};
    static const std::vector<int> t3 = {-1, -1, 2, 4, -1, -1, 2, 4};
    static const std::vector<int> t4 = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
    static const std::vector<int> t5 = {
        -1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16,
        -1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16};
    switch (bitsPerSample) {
    case 2:
        return t2;
    case 3:
        return t3;
    case 4:
        return t4;
    default:
        return t5;
    }
}

std::vector<int16_t> decodeSqueakSound(const std::vector<uint8_t> &data, int bitsPerSample) {
    if (bitsPerSample < 2 || bitsPerSample > 5 || data.empty()) return {};
    const std::vector<int> &indexTable = squeakIndexTable(bitsPerSample);
    int signMask = 1 << (bitsPerSample - 1);
    int valueHighBit = signMask >> 1;

    size_t bytePos = 0;
    int bitPosition = 0;
    uint8_t currentByte = 0;

    const auto nextCode = [&]() -> int {
        int remaining = bitsPerSample;
        int shift = remaining - bitPosition;
        int result = (shift < 0) ? (currentByte >> -shift) : (currentByte << shift);
        while (shift > 0) {
            remaining -= bitPosition;
            if (bytePos < data.size()) {
                currentByte = data[bytePos++];
                bitPosition = 8;
            } else {
                currentByte = 0;
                bitPosition = 0;
                return -1;
            }
            shift = remaining - bitPosition;
            result += (shift < 0) ? (currentByte >> -shift) : (currentByte << shift);
        }
        bitPosition -= remaining;
        currentByte = currentByte & (0xff >> (8 - bitPosition));
        return result;
    };

    size_t size = data.size() * 8 / bitsPerSample;
    std::vector<int16_t> result(size, 0);
    int sample = 0;
    int index = 0;

    for (size_t i = 0; i < size; i++) {
        int code = nextCode();
        if (code < 0) break;

        int step = squeakStepSizeTable[index];
        int delta = 0;
        for (int bit = valueHighBit; bit > 0; bit >>= 1) {
            if (code & bit) delta += step;
            step >>= 1;
        }
        delta += step;

        sample += ((code & signMask) == 0) ? delta : -delta;

        index += indexTable[code];
        if (index < 0) index = 0;
        if (index > 88) index = 88;

        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        result[i] = static_cast<int16_t>(sample);
    }
    return result;
}

void putU16le(std::vector<uint8_t> &out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
}
void putU32le(std::vector<uint8_t> &out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}
void putI32le(std::vector<uint8_t> &out, int32_t v) { putU32le(out, static_cast<uint32_t>(v)); }

std::vector<uint8_t> encodeBmp(int width, int height, const std::vector<uint32_t> &pixelsArgb) {
    std::vector<uint8_t> dib;
    putU32le(dib, 108);
    putI32le(dib, width);
    putI32le(dib, -height);
    putU16le(dib, 1);
    putU16le(dib, 32);
    putU32le(dib, 3);
    putU32le(dib, static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4);
    putU32le(dib, 0);
    putU32le(dib, 0);
    putU32le(dib, 0);
    putU32le(dib, 0);
    putU32le(dib, 0x00FF0000);
    putU32le(dib, 0x0000FF00);
    putU32le(dib, 0x000000FF);
    putU32le(dib, 0xFF000000);
    putU32le(dib, 0);
    for (int i = 0; i < 9; i++)
        putU32le(dib, 0);
    for (int i = 0; i < 3; i++)
        putU32le(dib, 0);

    std::vector<uint8_t> file;
    file.push_back('B');
    file.push_back('M');
    putU32le(file, static_cast<uint32_t>(14 + dib.size() + pixelsArgb.size() * 4));
    putU32le(file, 0);
    putU32le(file, static_cast<uint32_t>(14 + dib.size()));
    file.insert(file.end(), dib.begin(), dib.end());

    file.reserve(file.size() + pixelsArgb.size() * 4);
    for (uint32_t px : pixelsArgb) {
        file.push_back(static_cast<uint8_t>(px & 0xff));
        file.push_back(static_cast<uint8_t>((px >> 8) & 0xff));
        file.push_back(static_cast<uint8_t>((px >> 16) & 0xff));
        file.push_back(static_cast<uint8_t>((px >> 24) & 0xff));
    }
    return file;
}

std::vector<uint8_t> encodeWav(const std::vector<int16_t> &samples, int sampleRate) {
    uint32_t dataSize = static_cast<uint32_t>(samples.size() * 2);
    std::vector<uint8_t> out;
    out.reserve(44 + dataSize);

    out.push_back('R');
    out.push_back('I');
    out.push_back('F');
    out.push_back('F');
    putU32le(out, 36 + dataSize);
    out.push_back('W');
    out.push_back('A');
    out.push_back('V');
    out.push_back('E');

    out.push_back('f');
    out.push_back('m');
    out.push_back('t');
    out.push_back(' ');
    putU32le(out, 16);
    putU16le(out, 1);
    putU16le(out, 1);
    putU32le(out, static_cast<uint32_t>(sampleRate));
    putU32le(out, static_cast<uint32_t>(sampleRate) * 2);
    putU16le(out, 2);
    putU16le(out, 16);

    out.push_back('d');
    out.push_back('a');
    out.push_back('t');
    out.push_back('a');
    putU32le(out, dataSize);
    for (int16_t s : samples)
        putU16le(out, static_cast<uint16_t>(s));
    return out;
}

constexpr int spriteClassId = 124;
constexpr int stageClassId = 125;

std::string fieldString(const SqFieldPtr &f) {
    if (!f) return "";
    if (f->kind == SqField::Kind::String) return f->stringValue;
    if (f->kind == SqField::Kind::Number) return std::to_string(f->numberValue);
    return "";
}
double fieldNumber(const SqFieldPtr &f, double fallback = 0) {
    return (f && f->kind == SqField::Kind::Number) ? f->numberValue : fallback;
}
bool fieldBool(const SqFieldPtr &f, bool fallback = false) {
    return (f && f->kind == SqField::Kind::Bool) ? f->boolValue : fallback;
}

nlohmann::json fieldToJsonValue(const SqFieldPtr &f) {
    if (!f) return nullptr;
    switch (f->kind) {
    case SqField::Kind::Null:
        return nullptr;
    case SqField::Kind::Bool:
        return f->boolValue;
    case SqField::Kind::Number:
        return f->numberValue;
    case SqField::Kind::String:
        return f->stringValue;
    default:
        return fieldString(f);
    }
}

struct Sb1Assets {
    std::unordered_map<std::string, std::vector<uint8_t>> assets;
    int nextImageId = 0;
    int nextSoundId = 0;

    int reserveImageId() { return nextImageId++; }
    int reserveSoundId() { return nextSoundId++; }

    void storeBmp(int id, int width, int height, const std::vector<uint32_t> &pixels) {
        assets[std::to_string(id) + ".bmp"] = encodeBmp(width, height, pixels);
    }

    void storeWav(int id, const std::vector<int16_t> &samples, int sampleRate) {
        assets[std::to_string(id) + ".wav"] = encodeWav(samples, sampleRate > 0 ? sampleRate : 22050);
    }
};

nlohmann::json convertCostume(const SqFieldPtr &imageMedia, Sb1Assets &assets) {
    nlohmann::json costume;
    costume["costumeName"] = fieldString(imageMedia->field(0));
    costume["bitmapResolution"] = 1;

    SqFieldPtr rotationCenter = imageMedia->field(2);
    costume["rotationCenterX"] = rotationCenter ? fieldNumber(rotationCenter->field(0)) : 0;
    costume["rotationCenterY"] = rotationCenter ? fieldNumber(rotationCenter->field(1)) : 0;

    SqFieldPtr oldComposite = imageMedia->field(5);
    SqFieldPtr baseLayerData = imageMedia->field(4);
    SqFieldPtr bitmap = imageMedia->field(1);

    SqFieldPtr formToDecode;
    if (oldComposite && oldComposite->kind == SqField::Kind::Array) formToDecode = oldComposite;
    else if (!(baseLayerData && baseLayerData->kind == SqField::Kind::Bytes && !baseLayerData->bytesValue.empty())) formToDecode = bitmap;

    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
    if (formToDecode) {
        width = static_cast<int>(fieldNumber(formToDecode->field(0)));
        height = static_cast<int>(fieldNumber(formToDecode->field(1)));
        pixels = decodeSqueakImage(formToDecode);
    }

    int id = assets.reserveImageId();
    costume["baseLayerID"] = id;
    costume["baseLayerMD5"] = std::to_string(id) + ".bmp";
    if (!pixels.empty() && width > 0 && height > 0) {
        assets.storeBmp(id, width, height, pixels);
    } else {
        Log::logWarning("SB1 costume '" + fieldString(imageMedia->field(0)) + "' uses an unsupported encoding (e.g. JPEG) and will appear blank.");
        assets.storeBmp(id, 1, 1, {0});
    }
    return costume;
}

nlohmann::json convertSound(const SqFieldPtr &soundMedia, Sb1Assets &assets) {
    nlohmann::json sound;
    sound["soundName"] = fieldString(soundMedia->field(0));
    int id = assets.reserveSoundId();
    sound["soundID"] = id;
    sound["format"] = "";

    SqFieldPtr compressedData = soundMedia->field(6);
    SqFieldPtr uncompressed = soundMedia->field(1);

    std::vector<int16_t> samples;
    int rate = 22050;

    if (compressedData && compressedData->kind == SqField::Kind::Sound && !compressedData->bytesValue.empty()) {
        int bitsPerSample = static_cast<int>(fieldNumber(soundMedia->field(5), 4));
        samples = decodeSqueakSound(compressedData->bytesValue, bitsPerSample);
        rate = static_cast<int>(fieldNumber(soundMedia->field(4), 22050));
    } else if (uncompressed) {
        SqFieldPtr rawData = uncompressed->field(3);
        rate = static_cast<int>(fieldNumber(uncompressed->field(4), 22050));
        if (rawData && rawData->kind == SqField::Kind::Sound) {
            const auto &bytes = rawData->bytesValue;
            samples.resize(bytes.size() / 2);
            for (size_t i = 0; i < samples.size(); i++) {
                uint16_t v = static_cast<uint16_t>((bytes[i * 2] << 8) | bytes[i * 2 + 1]);
                samples[i] = static_cast<int16_t>(v);
            }
        }
    }

    sound["sampleCount"] = static_cast<int>(samples.size());
    sound["rate"] = rate;
    sound["md5"] = std::to_string(id) + ".wav";
    assets.storeWav(id, samples, rate);
    return sound;
}

nlohmann::json convertVariables(const SqFieldPtr &pairs) {
    nlohmann::json out = nlohmann::json::array();
    if (!pairs) return out;
    for (size_t i = 0; i + 1 < pairs->items.size(); i += 2) {
        nlohmann::json v;
        v["name"] = fieldString(pairs->field(i));
        v["value"] = fieldToJsonValue(pairs->field(i + 1));
        out.push_back(v);
    }
    return out;
}

nlohmann::json convertLists(const SqFieldPtr &pairs) {
    nlohmann::json out = nlohmann::json::array();
    if (!pairs) return out;
    for (size_t i = 0; i + 1 < pairs->items.size(); i += 2) {
        SqFieldPtr listWatcher = pairs->field(i + 1);
        if (!listWatcher) continue;
        nlohmann::json l;
        l["listName"] = fieldString(listWatcher->field(8));
        nlohmann::json contents = nlohmann::json::array();
        SqFieldPtr contentsField = listWatcher->field(9);
        if (contentsField) {
            for (const auto &item : contentsField->items)
                contents.push_back(fieldToJsonValue(item));
        }
        l["contents"] = contents;
        out.push_back(l);
    }
    return out;
}

void fixSb1BlockOpcode(nlohmann::json &block) {
    if (!block.is_array() || block.empty() || !block[0].is_string()) return;
    std::string op = block[0].get<std::string>();

    if (op == "EventHatMorph") {
        std::string arg = block.size() > 1 && block[1].is_string() ? block[1].get<std::string>() : "";
        if (arg == "Scratch-StartClicked") block = {"whenGreenFlag"};
        else block = {"whenIReceive", arg};
    } else if (op == "MouseClickEventHatMorph") {
        block = {"whenClicked"};
    } else if (op == "KeyEventHatMorph") {
        block = {"whenKeyPressed", block.size() > 1 ? block[1] : nlohmann::json("")};
    } else if (op == "stopScripts") {
        if (block.size() > 1 && block[1] == "other scripts") block[1] = "other scripts in sprite";
    } else if (op == "getParam") {
        while (block.size() < 4)
            block.push_back(nullptr);
        if (block[3].is_null()) block[3] = "r";
    } else if (op == "changeVariable") {
        if (block.size() > 3) block = {block[2], block[1], block[3]};
    } else if (op == "abs") {
        block = {"computeFunction:of:", "abs", block.size() > 1 ? block[1] : nlohmann::json(0)};
    } else if (op == "sqrt") {
        block = {"computeFunction:of:", "sqrt", block.size() > 1 ? block[1] : nlohmann::json(0)};
    } else if (op == "\\\\") {
        nlohmann::json rest = nlohmann::json::array();
        for (size_t i = 1; i < block.size(); i++)
            rest.push_back(block[i]);
        nlohmann::json newBlock = nlohmann::json::array();
        newBlock.push_back("%");
        for (auto &a : rest)
            newBlock.push_back(a);
        block = newBlock;
    } else if (op == "doReturn") {
        block = {"stopScripts", "this script"};
    } else if (op == "stopAll") {
        block = {"stopScripts", "all"};
    } else if (op == "showBackground:") {
        block = {"startScene", block.size() > 1 ? block[1] : nlohmann::json("")};
    } else if (op == "nextBackground") {
        block = {"nextScene"};
    } else if (op == "doForeverIf") {
        nlohmann::json innerIf = nlohmann::json::array();
        innerIf.push_back("doIf");
        innerIf.push_back(block.size() > 1 ? block[1] : nlohmann::json(false));
        innerIf.push_back(block.size() > 2 ? block[2] : nlohmann::json::array());
        nlohmann::json innerStack = nlohmann::json::array();
        innerStack.push_back(innerIf);
        block = {"doForever", innerStack};
    } else if (op == "getAttribute:of:" || op == "gotoSpriteOrMouse:" || op == "distanceTo:" ||
               op == "pointTowards:" || op == "touching:") {
        size_t last = block.size() - 1;
        if (block[last].is_string()) {
            std::string v = block[last].get<std::string>();
            if (v == "mouse") block[last] = "_mouse_";
            else if (v == "edge") block[last] = "_edge_";
            else if (v == "Stage") block[last] = "_stage_";
        }
    }
}

void fixSb1BlockTree(nlohmann::json &node) {
    if (!node.is_array()) return;
    if (!node.empty() && node[0].is_array()) {
        for (auto &block : node)
            fixSb1BlockTree(block);
        return;
    }
    fixSb1BlockOpcode(node);
    for (size_t i = 1; i < node.size(); i++)
        fixSb1BlockTree(node[i]);
}

nlohmann::json convertScript(const SqFieldPtr &script) {
    nlohmann::json out = nlohmann::json::array();
    if (!script || script->items.size() < 2) return out;
    SqFieldPtr position = script->field(0);
    SqFieldPtr stack = script->field(1);

    out.push_back(position ? fieldNumber(position->field(0)) : 0);
    out.push_back(position ? fieldNumber(position->field(1)) : 0);

    nlohmann::json blockStack = nlohmann::json::array();
    const std::function<nlohmann::json(const SqFieldPtr &)> convertArg = [&](const SqFieldPtr &arg) -> nlohmann::json {
        if (!arg) return nullptr;
        if (arg->kind == SqField::Kind::Object && (arg->classId == spriteClassId || arg->classId == stageClassId)) {
            return fieldString(arg->field(6));
        }
        if (arg->kind == SqField::Kind::Array) {
            if (arg->items.empty() || (arg->field(0) && arg->field(0)->kind == SqField::Kind::Array)) {
                nlohmann::json stackJson = nlohmann::json::array();
                for (const auto &b : arg->items)
                    stackJson.push_back(convertArg(b));
                return stackJson;
            }
            nlohmann::json blockJson = nlohmann::json::array();
            for (const auto &a : arg->items)
                blockJson.push_back(convertArg(a));
            return blockJson;
        }
        return fieldToJsonValue(arg);
    };

    if (stack) {
        for (const auto &block : stack->items)
            blockStack.push_back(convertArg(block));
    }
    fixSb1BlockTree(blockStack);
    out.push_back(blockStack);
    return out;
}

nlohmann::json convertScripts(const SqFieldPtr &blocksBin) {
    nlohmann::json out = nlohmann::json::array();
    if (!blocksBin) return out;
    for (const auto &script : blocksBin->items)
        out.push_back(convertScript(script));
    return out;
}

constexpr int objNameFieldIndex = 6;
constexpr int varsFieldIndex = 7;
constexpr int blocksBinFieldIndex = 8;
constexpr int mediaFieldIndex = 10;
constexpr int currentCostumeFieldIndex = 11;
constexpr int listsFieldIndex = 20;
constexpr int boxFieldIndex = 0;
constexpr int visibleFieldIndex = 4;
constexpr int scalePointFieldIndex = 13;
constexpr int rotationDegreesFieldIndex = 14;
constexpr int rotationStyleFieldIndex = 15;
constexpr int draggableFieldIndex = 18;
constexpr int stageContentsFieldIndex = 2;

void convertMedia(const SqFieldPtr &object, Sb1Assets &assets, nlohmann::json &outCostumes, nlohmann::json &outSounds, int &outCurrentCostumeIndex) {
    outCostumes = nlohmann::json::array();
    outSounds = nlohmann::json::array();
    outCurrentCostumeIndex = 0;

    SqFieldPtr media = object->field(mediaFieldIndex);
    SqFieldPtr currentCostume = object->field(currentCostumeFieldIndex);
    if (!media) return;

    int costumeIndex = 0;
    int soundIndex = 0;
    int currentIndex = -1;
    for (const auto &item : media->items) {
        if (!item || item->kind != SqField::Kind::Object) continue;
        constexpr int imageMediaClassId = 162;
        constexpr int soundMediaClassId = 164;
        if (item->classId == imageMediaClassId) {
            if (item.get() == currentCostume.get()) currentIndex = costumeIndex;
            outCostumes.push_back(convertCostume(item, assets));
            costumeIndex++;
        } else if (item->classId == soundMediaClassId) {
            outSounds.push_back(convertSound(item, assets));
            soundIndex++;
        }
    }
    outCurrentCostumeIndex = currentIndex >= 0 ? currentIndex : 0;
}

nlohmann::json convertSprite(const SqFieldPtr &sprite, Sb1Assets &assets) {
    nlohmann::json json;
    json["objName"] = fieldString(sprite->field(objNameFieldIndex));
    json["visible"] = (static_cast<int>(fieldNumber(sprite->field(visibleFieldIndex), 0)) & 1) == 0;
    json["isDraggable"] = fieldBool(sprite->field(draggableFieldIndex));

    SqFieldPtr box = sprite->field(boxFieldIndex);
    SqFieldPtr currentCostume = sprite->field(currentCostumeFieldIndex);
    SqFieldPtr rotationCenter = currentCostume ? currentCostume->field(2) : nullptr;
    double boxX = box ? fieldNumber(box->field(0)) : 0;
    double boxY = box ? fieldNumber(box->field(1)) : 0;
    double rcX = rotationCenter ? fieldNumber(rotationCenter->field(0)) : 0;
    double rcY = rotationCenter ? fieldNumber(rotationCenter->field(1)) : 0;
    json["scratchX"] = boxX + rcX - 240;
    json["scratchY"] = 180 - (boxY + rcY);

    SqFieldPtr scalePoint = sprite->field(scalePointFieldIndex);
    json["scale"] = scalePoint ? fieldNumber(scalePoint->field(0), 1) : 1;
    json["direction"] = fieldNumber(sprite->field(rotationDegreesFieldIndex)) - 270;
    json["rotationStyle"] = fieldString(sprite->field(rotationStyleFieldIndex));
    json["currentCostumeIndex"] = 0;

    nlohmann::json costumes, sounds;
    int currentCostumeIndex = 0;
    convertMedia(sprite, assets, costumes, sounds, currentCostumeIndex);
    json["costumes"] = costumes;
    json["sounds"] = sounds;
    json["currentCostumeIndex"] = currentCostumeIndex;

    json["variables"] = convertVariables(sprite->field(varsFieldIndex));
    json["lists"] = convertLists(sprite->field(listsFieldIndex));
    json["scripts"] = convertScripts(sprite->field(blocksBinFieldIndex));
    return json;
}

nlohmann::json convertStage(const SqFieldPtr &stage, Sb1Assets &assets) {
    nlohmann::json json;
    json["objName"] = fieldString(stage->field(objNameFieldIndex));

    nlohmann::json costumes, sounds;
    int currentCostumeIndex = 0;
    convertMedia(stage, assets, costumes, sounds, currentCostumeIndex);
    json["costumes"] = costumes;
    json["sounds"] = sounds;
    json["currentCostumeIndex"] = currentCostumeIndex;

    json["variables"] = convertVariables(stage->field(varsFieldIndex));
    json["lists"] = convertLists(stage->field(listsFieldIndex));
    json["scripts"] = convertScripts(stage->field(blocksBinFieldIndex));

    nlohmann::json children = nlohmann::json::array();
    SqFieldPtr stageContents = stage->field(stageContentsFieldIndex);
    if (stageContents) {
        for (auto it = stageContents->items.rbegin(); it != stageContents->items.rend(); ++it) {
            const SqFieldPtr &child = *it;
            if (child && child->kind == SqField::Kind::Object && child->classId == spriteClassId) {
                children.push_back(convertSprite(child, assets));
            }
        }
    }
    json["children"] = children;
    return json;
}

} // namespace

namespace {
class Sb1Loader : public ProjectLoader {
  public:
    bool load(std::istream *file) override {
        if (!file) return false;

        file->seekg(0, std::ios::end);
        std::streamsize size = file->tellg();
        file->seekg(0, std::ios::beg);
        if (size <= 0) return false;

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (!file->read(reinterpret_cast<char *>(buffer.data()), size)) return false;

        SqFieldPtr stage = parseSb1File(buffer);
        if (!stage) {
            Log::logCritical("Failed to parse .sb (Scratch 1.4) project.", false);
            return false;
        }

        nlohmann::json root = convertStage(stage, assets);
        Parser::loadSprites(root, ProjectFormat::SB2);
        return true;
    }

    void *getAsset(const std::string &name, size_t *outSize) override {
        auto it = assets.assets.find(name);
        if (it == assets.assets.end()) return nullptr;
        void *buf = std::malloc(it->second.size());
        if (!buf) return nullptr;
        std::memcpy(buf, it->second.data(), it->second.size());
        if (outSize) *outSize = it->second.size();
        return buf;
    }

  private:
    Sb1Assets assets;
};
} // namespace

std::unique_ptr<ProjectLoader> createSb1Loader() {
    return std::make_unique<Sb1Loader>();
}
