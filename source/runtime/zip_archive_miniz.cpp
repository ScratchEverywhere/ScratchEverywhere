#include "zip_archive.hpp"

#include <cstring>
#include <istream>
#include <miniz.h>

namespace {

class MinizZipArchive : public ZipArchive {
  public:
    ~MinizZipArchive() override {
        if (opened) mz_zip_reader_end(&archive);
    }

    bool openFile(const std::string &path) override {
        memset(&archive, 0, sizeof(archive));
        opened = mz_zip_reader_init_file(&archive, path.c_str(), 0);
        return opened;
    }

    bool openMemory(const void *data, size_t size) override {
        memset(&archive, 0, sizeof(archive));
        opened = mz_zip_reader_init_mem(&archive, data, size, 0);
        return opened;
    }

    bool openStream(std::istream *stream, uint64_t size) override {
        memset(&archive, 0, sizeof(archive));
        archive.m_pIO_opaque = stream;
        archive.m_pRead = &MinizZipArchive::streamReadFunc;
        opened = mz_zip_reader_init(&archive, size, 0);
        return opened;
    }

    int getNumFiles() const override {
        if (!opened) return 0;
        return static_cast<int>(mz_zip_reader_get_num_files(const_cast<mz_zip_archive *>(&archive)));
    }

    int locateFile(const std::string &name) const override {
        if (!opened) return -1;
        return mz_zip_reader_locate_file(const_cast<mz_zip_archive *>(&archive), name.c_str(), nullptr, 0);
    }

    bool getFilename(int index, std::string &outName) const override {
        if (!opened) return false;
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(const_cast<mz_zip_archive *>(&archive), index, &st)) return false;
        outName = st.m_filename;
        return true;
    }

    void *extractToHeap(int index, size_t *outSize) override {
        if (!opened) return nullptr;
        return mz_zip_reader_extract_to_heap(&archive, index, outSize, 0);
    }

    void freeHeap(void *ptr) override {
        mz_free(ptr);
    }

    bool extractToFile(int index, const std::string &outPath) override {
        if (!opened) return false;
        return mz_zip_reader_extract_to_file(&archive, index, outPath.c_str(), 0);
    }

  private:
    static size_t streamReadFunc(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n) {
        std::istream *stream = static_cast<std::istream *>(pOpaque);
        stream->clear();
        stream->seekg(file_ofs, std::ios::beg);
        stream->read(static_cast<char *>(pBuf), n);
        return static_cast<size_t>(stream->gcount());
    }

    mz_zip_archive archive{};
    bool opened = false;
};

} // namespace

std::unique_ptr<ZipArchive> createZipArchive() {
    return std::make_unique<MinizZipArchive>();
}
