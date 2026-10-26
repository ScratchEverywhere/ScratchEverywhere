#include "bytecode_cache.hpp"
#include <blockExecutor.hpp>
#include <cstring>
#include <filesystem.hpp>
#include <fstream>
#include <log.hpp>
#include <os.hpp>
#include <parser.hpp>
#include <runtime.hpp>
#include <sstream>
#include <types.hpp>
#include <unzip.hpp>

#ifdef USE_BUNDLE
#include <bundle_archives.hpp>
#endif

namespace {

constexpr char cacheMagic[4] = {'S', 'E', 'B', 'C'};

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
constexpr bool hostIsBigEndian = true;
#else
constexpr bool hostIsBigEndian = false;
#endif

uint16_t byteSwap16(uint16_t v) { return static_cast<uint16_t>((v >> 8) | (v << 8)); }
uint32_t byteSwap32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) | ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}
uint64_t byteSwap64(uint64_t v) {
    return ((v & 0x00000000000000FFull) << 56) | ((v & 0x000000000000FF00ull) << 40) |
           ((v & 0x0000000000FF0000ull) << 24) | ((v & 0x00000000FF000000ull) << 8) |
           ((v & 0x000000FF00000000ull) >> 8) | ((v & 0x0000FF0000000000ull) >> 24) |
           ((v & 0x00FF000000000000ull) >> 40) | ((v & 0xFF00000000000000ull) >> 56);
}

uint16_t swapToLE16(uint16_t v) { return hostIsBigEndian ? byteSwap16(v) : v; }
uint32_t swapToLE32(uint32_t v) { return hostIsBigEndian ? byteSwap32(v) : v; }
uint64_t swapToLE64(uint64_t v) { return hostIsBigEndian ? byteSwap64(v) : v; }

void writeU8(std::ostream &out, uint8_t v) { out.write(reinterpret_cast<const char *>(&v), sizeof(v)); }
void writeU16(std::ostream &out, uint16_t v) {
    uint16_t le = swapToLE16(v);
    out.write(reinterpret_cast<const char *>(&le), sizeof(le));
}
void writeU32(std::ostream &out, uint32_t v) {
    uint32_t le = swapToLE32(v);
    out.write(reinterpret_cast<const char *>(&le), sizeof(le));
}
void writeDouble(std::ostream &out, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = swapToLE64(bits);
    out.write(reinterpret_cast<const char *>(&bits), sizeof(bits));
}
void writeFloat(std::ostream &out, float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = swapToLE32(bits);
    out.write(reinterpret_cast<const char *>(&bits), sizeof(bits));
}
void writeBool(std::ostream &out, bool v) { writeU8(out, v ? 1 : 0); }
void writeString(std::ostream &out, const std::string &s) {
    writeU32(out, static_cast<uint32_t>(s.size()));
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
}
void writeColor(std::ostream &out, const Color &c) {
    writeFloat(out, c.hue);
    writeFloat(out, c.saturation);
    writeFloat(out, c.brightness);
    writeFloat(out, c.transparency);
}
void writeValue(std::ostream &out, const Value &v) {
    writeU8(out, static_cast<uint8_t>(v.getTag()));
    switch (v.getTag()) {
    case Value::Tag::UNDEFINED:
        break;
    case Value::Tag::BOOLEAN:
        writeBool(out, v.get<bool>());
        break;
    case Value::Tag::NUMBER:
        writeDouble(out, v.get<double>());
        break;
    case Value::Tag::STRING_ID:
        writeString(out, v.get<std::string>());
        break;
    case Value::Tag::COLOR:
        writeColor(out, v.asColor());
        break;
    }
}
void writeIndexVec(std::ostream &out, const std::vector<BytecodeIndex> &v) {
    writeU32(out, static_cast<uint32_t>(v.size()));
    for (BytecodeIndex i : v)
        writeU32(out, i);
}

bool readU8(std::istream &in, uint8_t &v) { return static_cast<bool>(in.read(reinterpret_cast<char *>(&v), sizeof(v))); }
bool readU16(std::istream &in, uint16_t &v) {
    uint16_t raw;
    if (!in.read(reinterpret_cast<char *>(&raw), sizeof(raw))) return false;
    v = swapToLE16(raw);
    return true;
}
bool readU32(std::istream &in, uint32_t &v) {
    uint32_t raw;
    if (!in.read(reinterpret_cast<char *>(&raw), sizeof(raw))) return false;
    v = swapToLE32(raw);
    return true;
}
bool readDouble(std::istream &in, double &v) {
    uint64_t bits;
    if (!in.read(reinterpret_cast<char *>(&bits), sizeof(bits))) return false;
    bits = swapToLE64(bits);
    std::memcpy(&v, &bits, sizeof(v));
    return true;
}
bool readFloat(std::istream &in, float &v) {
    uint32_t bits;
    if (!in.read(reinterpret_cast<char *>(&bits), sizeof(bits))) return false;
    bits = swapToLE32(bits);
    std::memcpy(&v, &bits, sizeof(v));
    return true;
}
bool readBool(std::istream &in, bool &v) {
    uint8_t raw;
    if (!readU8(in, raw)) return false;
    v = raw != 0;
    return true;
}
bool readString(std::istream &in, std::string &s) {
    uint32_t len;
    if (!readU32(in, len)) return false;
    if (len > (1u << 28)) return false;
    s.resize(len);
    if (len == 0) return true;
    return static_cast<bool>(in.read(s.data(), static_cast<std::streamsize>(len)));
}
bool readColor(std::istream &in, Color &c) {
    return readFloat(in, c.hue) && readFloat(in, c.saturation) && readFloat(in, c.brightness) && readFloat(in, c.transparency);
}
bool readValue(std::istream &in, Value &out) {
    uint8_t tagRaw;
    if (!readU8(in, tagRaw)) return false;
    switch (static_cast<Value::Tag>(tagRaw)) {
    case Value::Tag::UNDEFINED:
        out = Value(Undefined{});
        return true;
    case Value::Tag::BOOLEAN: {
        bool b;
        if (!readBool(in, b)) return false;
        out = Value(b);
        return true;
    }
    case Value::Tag::NUMBER: {
        double d;
        if (!readDouble(in, d)) return false;
        out = Value(d);
        return true;
    }
    case Value::Tag::STRING_ID: {
        std::string s;
        if (!readString(in, s)) return false;
        out = Value(s);
        return true;
    }
    case Value::Tag::COLOR: {
        Color c;
        if (!readColor(in, c)) return false;
        out = Value(c);
        return true;
    }
    }
    return false;
}
bool readIndexVec(std::istream &in, std::vector<BytecodeIndex> &v) {
    uint32_t count;
    if (!readU32(in, count)) return false;
    v.clear();
    v.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        BytecodeIndex idx;
        if (!readU32(in, idx)) return false;
        v.push_back(idx);
    }
    return true;
}

void writeChunk(std::ostream &out, const BytecodeChunk &chunk) {
    writeU32(out, static_cast<uint32_t>(chunk.instructions.size()));
    for (const BytecodeInstruction &instr : chunk.instructions) {
        writeU8(out, static_cast<uint8_t>(instr.op));
        writeU32(out, instr.a);
        writeU32(out, instr.b);
    }

    writeU32(out, static_cast<uint32_t>(chunk.numberConstants.size()));
    for (double d : chunk.numberConstants)
        writeDouble(out, d);

    writeU32(out, static_cast<uint32_t>(chunk.stringConstants.size()));
    for (const std::string &s : chunk.stringConstants)
        writeString(out, s);

    writeU32(out, static_cast<uint32_t>(chunk.boolConstants.size()));
    for (bool b : chunk.boolConstants)
        writeBool(out, b);

    writeU32(out, static_cast<uint32_t>(chunk.colorConstants.size()));
    for (const Color &c : chunk.colorConstants)
        writeColor(out, c);

    writeU32(out, static_cast<uint32_t>(chunk.constants.size()));
    for (const Value &v : chunk.constants)
        writeValue(out, v);

    writeU32(out, static_cast<uint32_t>(chunk.strings.size()));
    for (const std::string &s : chunk.strings)
        writeString(out, s);

    writeIndexVec(out, chunk.opcodeSlots);
    writeIndexVec(out, chunk.variableSlots);
    writeIndexVec(out, chunk.listSlots);

    writeU32(out, static_cast<uint32_t>(chunk.paramKinds.size()));
    for (BytecodeStackKind k : chunk.paramKinds)
        writeU8(out, static_cast<uint8_t>(k));
    writeIndexVec(out, chunk.paramNames);
    writeIndexVec(out, chunk.paramKindsStart);
    writeIndexVec(out, chunk.paramKindsCount);

    writeU32(out, static_cast<uint32_t>(chunk.procedures.size()));
    for (const BytecodeProcedure &p : chunk.procedures) {
        writeU32(out, p.procCodeIndex);
        writeU32(out, p.entryInstruction);
        writeU32(out, p.argCount);
        writeBool(out, p.runWithoutScreenRefresh);
        writeU32(out, p.argumentNamesStart);
        writeU32(out, p.argumentNamesCount);
    }

    writeU32(out, static_cast<uint32_t>(chunk.scripts.size()));
    for (const BytecodeScript &s : chunk.scripts) {
        writeU32(out, s.opcodeIndex);
        writeU32(out, s.entryInstruction);
        writeU32(out, s.blockOrdinal);
    }

    writeIndexVec(out, chunk.escapeBlockOrdinals);
}

bool readChunk(std::istream &in, BytecodeChunk &chunk) {
    uint32_t count;

    if (!readU32(in, count)) return false;
    chunk.instructions.clear();
    chunk.instructions.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t opRaw;
        uint32_t a, b;
        if (!readU8(in, opRaw) || !readU32(in, a) || !readU32(in, b)) return false;
        chunk.instructions.push_back(BytecodeInstruction{static_cast<BytecodeOp>(opRaw), a, b});
    }

    if (!readU32(in, count)) return false;
    chunk.numberConstants.clear();
    chunk.numberConstants.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        double d;
        if (!readDouble(in, d)) return false;
        chunk.numberConstants.push_back(d);
    }

    if (!readU32(in, count)) return false;
    chunk.stringConstants.clear();
    chunk.stringConstants.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        std::string s;
        if (!readString(in, s)) return false;
        chunk.stringConstants.push_back(std::move(s));
    }

    if (!readU32(in, count)) return false;
    chunk.boolConstants.clear();
    chunk.boolConstants.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        bool b;
        if (!readBool(in, b)) return false;
        chunk.boolConstants.push_back(b);
    }

    if (!readU32(in, count)) return false;
    chunk.colorConstants.clear();
    chunk.colorConstants.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        Color c;
        if (!readColor(in, c)) return false;
        chunk.colorConstants.push_back(c);
    }

    if (!readU32(in, count)) return false;
    chunk.constants.clear();
    chunk.constants.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        Value v;
        if (!readValue(in, v)) return false;
        chunk.constants.push_back(std::move(v));
    }

    if (!readU32(in, count)) return false;
    chunk.strings.clear();
    chunk.strings.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        std::string s;
        if (!readString(in, s)) return false;
        chunk.strings.push_back(std::move(s));
    }

    if (!readIndexVec(in, chunk.opcodeSlots)) return false;
    if (!readIndexVec(in, chunk.variableSlots)) return false;
    if (!readIndexVec(in, chunk.listSlots)) return false;

    if (!readU32(in, count)) return false;
    chunk.paramKinds.clear();
    chunk.paramKinds.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t k;
        if (!readU8(in, k)) return false;
        chunk.paramKinds.push_back(static_cast<BytecodeStackKind>(k));
    }
    if (!readIndexVec(in, chunk.paramNames)) return false;
    if (!readIndexVec(in, chunk.paramKindsStart)) return false;
    if (!readIndexVec(in, chunk.paramKindsCount)) return false;

    if (!readU32(in, count)) return false;
    chunk.procedures.clear();
    chunk.procedures.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        BytecodeProcedure p;
        bool runWithoutScreenRefresh;
        if (!readU32(in, p.procCodeIndex) || !readU32(in, p.entryInstruction) || !readU32(in, p.argCount) ||
            !readBool(in, runWithoutScreenRefresh) || !readU32(in, p.argumentNamesStart) || !readU32(in, p.argumentNamesCount))
            return false;
        p.runWithoutScreenRefresh = runWithoutScreenRefresh;
        chunk.procedures.push_back(p);
    }

    if (!readU32(in, count)) return false;
    chunk.scripts.clear();
    chunk.scripts.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        BytecodeScript s;
        if (!readU32(in, s.opcodeIndex) || !readU32(in, s.entryInstruction) || !readU32(in, s.blockOrdinal)) return false;
        chunk.scripts.push_back(s);
    }

    if (!readIndexVec(in, chunk.escapeBlockOrdinals)) return false;

    return true;
}

struct CachedSpriteEntry {
    std::string name;
    uint32_t blockOrdinalBase = 0;
    uint32_t blockCount = 0;
    BytecodeChunk chunk;
    bool consumed = false;
};

enum class LoadStatus {
    Ok,
    Missing,
    TooNew,
    TooOld,
};

LoadStatus loadCacheStream(std::istream &in, std::vector<CachedSpriteEntry> &outEntries) {
    char magic[4];
    if (!in.read(magic, 4) || std::memcmp(magic, cacheMagic, 4) != 0) return LoadStatus::Missing;

    uint16_t version;
    if (!readU16(in, version)) return LoadStatus::Missing;
    if (version > bytecodeCacheVersion) return LoadStatus::TooNew;
    if (version < bytecodeCacheVersion) return LoadStatus::TooOld;

    uint16_t spriteCount;
    if (!readU16(in, spriteCount)) return LoadStatus::Missing;

    std::vector<CachedSpriteEntry> entries;
    entries.reserve(spriteCount);
    for (uint16_t i = 0; i < spriteCount; i++) {
        CachedSpriteEntry entry;
        if (!readString(in, entry.name)) return LoadStatus::Missing;
        if (!readU32(in, entry.blockOrdinalBase)) return LoadStatus::Missing;
        if (!readU32(in, entry.blockCount)) return LoadStatus::Missing;
        if (!readChunk(in, entry.chunk)) return LoadStatus::Missing;
        entries.push_back(std::move(entry));
    }

    outEntries = std::move(entries);
    return LoadStatus::Ok;
}

LoadStatus loadCacheFile(const std::string &path, std::vector<CachedSpriteEntry> &outEntries) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return LoadStatus::Missing;
    return loadCacheStream(in, outEntries);
}

#ifdef USE_BUNDLE
LoadStatus loadCacheFromBundle(const std::string &bundlePath, std::vector<CachedSpriteEntry> &outEntries) {
    auto project = Bundle::project();
    auto entry = project.find(bundlePath);
    if (!entry) return LoadStatus::Missing;

    std::vector<std::byte> bytes = project.load(*entry);
    std::string blob(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    std::istringstream in(blob, std::ios::binary);
    return loadCacheStream(in, outEntries);
}
#endif

bool saveCacheFile(const std::string &path, const std::vector<std::tuple<std::string, uint32_t, uint32_t, const BytecodeChunk *>> &sprites) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out.write(cacheMagic, 4);
    writeU16(out, bytecodeCacheVersion);
    writeU16(out, static_cast<uint16_t>(sprites.size()));
    for (const auto &[name, base, blockCount, chunk] : sprites) {
        writeString(out, name);
        writeU32(out, base);
        writeU32(out, blockCount);
        writeChunk(out, *chunk);
    }
    return static_cast<bool>(out);
}

bool active_ = false;
bool writable_ = true;
bool dirty_ = false;
LoadStatus status_ = LoadStatus::Missing;
std::string cachePath_;
std::vector<CachedSpriteEntry> entries_;

struct SpriteRecord {
    Sprite *sprite;
    uint32_t base;
    uint32_t count;
};
std::vector<SpriteRecord> records_;

} // namespace

bool Parser::relinkBytecode(Sprite *sprite, BytecodeChunk &&chunkIn, uint32_t blockOrdinalBase, uint32_t blockOrdinalEnd) {
    auto chunk = std::make_unique<BytecodeChunk>(std::move(chunkIn));
    auto link = std::make_unique<BytecodeLinkTable>();

    auto &handlers = BlockExecutor::getHandlers();
    auto &positional = BlockExecutor::getPositionalHandlers();

    link->opcodes.reserve(chunk->opcodeSlots.size());
    link->positional.reserve(chunk->opcodeSlots.size());
    for (BytecodeIndex nameIdx : chunk->opcodeSlots) {
        if (nameIdx >= chunk->strings.size()) return false;
        const std::string &opcode = chunk->strings[nameIdx];
        auto hIt = handlers.find(opcode);
        auto pIt = positional.find(opcode);
        link->opcodes.push_back(hIt != handlers.end() ? &hIt->second : nullptr);
        link->positional.push_back(pIt != positional.end() ? pIt->second : BytecodePositionalFn{});
    }

    link->variables.reserve(chunk->variableSlots.size());
    for (BytecodeIndex nameIdx : chunk->variableSlots) {
        if (nameIdx >= chunk->strings.size()) return false;
        link->variables.push_back(BlockExecutor::getVariable(chunk->strings[nameIdx], sprite));
    }

    link->lists.reserve(chunk->listSlots.size());
    for (BytecodeIndex nameIdx : chunk->listSlots) {
        if (nameIdx >= chunk->strings.size()) return false;
        const std::string &listId = chunk->strings[nameIdx];
        List *list = nullptr;
        auto it = sprite->lists.find(listId);
        if (it != sprite->lists.end()) {
            list = &it->second;
        } else if (Scratch::stageSprite != nullptr) {
            auto globalIt = Scratch::stageSprite->lists.find(listId);
            if (globalIt != Scratch::stageSprite->lists.end()) list = &globalIt->second;
        }
        link->lists.push_back(list);
    }

    const auto resolveBlock = [&](BytecodeIndex ordinal) -> Block * {
        if (ordinal < blockOrdinalBase || ordinal >= blockOrdinalEnd) return nullptr;
        if (ordinal >= Scratch::blocks.size()) return nullptr;
        return Scratch::blocks[ordinal];
    };

    link->escapeBlocks.reserve(chunk->escapeBlockOrdinals.size());
    for (BytecodeIndex ordinal : chunk->escapeBlockOrdinals) {
        Block *block = resolveBlock(ordinal);
        if (block == nullptr) return false;
        link->escapeBlocks.push_back(block);
    }

    link->scriptBlocks.reserve(chunk->scripts.size());
    for (const BytecodeScript &script : chunk->scripts) {
        Block *block = resolveBlock(script.blockOrdinal);
        if (block == nullptr) return false;
        link->scriptBlocks.push_back(block);
    }

    sprite->bytecode = std::move(chunk);
    sprite->bytecodeLink = std::move(link);
    return true;
}

void BytecodeCache::beginLoadSession() {
    active_ = Scratch::useBytecode && Scratch::projectType == ProjectType::UNZIPPED;
    writable_ = true;
    dirty_ = false;
    entries_.clear();
    records_.clear();
    status_ = LoadStatus::Missing;
    if (!active_) return;

    if (Unzip::UnpackedInSD) {
        cachePath_ = Unzip::filePath + "bytecode.cache";
        status_ = loadCacheFile(cachePath_, entries_);
        return;
    }

    const std::string bakedPath = OS::getRomFSLocation() + "project/bytecode.cache";
#ifdef USE_BUNDLE
    status_ = loadCacheFromBundle(bakedPath, entries_);
#else
    status_ = loadCacheFile(bakedPath, entries_);
#endif
    if (status_ == LoadStatus::Ok) {
        writable_ = false;
        return;
    }

    auto potentialError = FileSystem::createDirectory(OS::getConfigFolderLocation());
    (void)potentialError;
    cachePath_ = OS::getConfigFolderLocation() + "bytecode.cache";
    status_ = loadCacheFile(cachePath_, entries_);
}

void BytecodeCache::processSprite(Sprite *sprite, uint32_t blockOrdinalBase, uint32_t blockOrdinalEnd) {
    if (!active_) {
        Parser::compileBytecode(sprite);
        return;
    }

    bool usedCache = false;
    if (status_ == LoadStatus::Ok) {
        const uint32_t count = blockOrdinalEnd - blockOrdinalBase;
        for (CachedSpriteEntry &entry : entries_) {
            if (entry.consumed || entry.name != sprite->name) continue;
            if (entry.blockOrdinalBase != blockOrdinalBase || entry.blockCount != count) continue;

            if (Parser::relinkBytecode(sprite, std::move(entry.chunk), blockOrdinalBase, blockOrdinalEnd)) {
                entry.consumed = true;
                usedCache = true;
            } else {
                Log::logWarning("Bytecode cache entry for sprite '" + sprite->name + "' failed to relink, recompiling.");
            }
            break;
        }
    }

    if (!usedCache) {
        Parser::compileBytecode(sprite);
        dirty_ = true;
    }

    records_.push_back({sprite, blockOrdinalBase, blockOrdinalEnd - blockOrdinalBase});
}

void BytecodeCache::endLoadSession() {
    if (!active_) return;

    if (!writable_) {
        return;
    }

    if (status_ == LoadStatus::TooNew) {
        Scratch::bytecodeCacheTooNew = true;
        return;
    }

    const bool needsRewrite = dirty_ || status_ != LoadStatus::Ok;
    if (!needsRewrite) return;

    std::vector<std::tuple<std::string, uint32_t, uint32_t, const BytecodeChunk *>> toWrite;
    toWrite.reserve(records_.size());
    for (const SpriteRecord &rec : records_) {
        if (rec.sprite->bytecode == nullptr) continue;
        toWrite.emplace_back(rec.sprite->name, rec.base, rec.count, rec.sprite->bytecode.get());
    }

    if (!saveCacheFile(cachePath_, toWrite)) {
        Log::logWarning("Failed to write bytecode cache: " + cachePath_);
    }
}
