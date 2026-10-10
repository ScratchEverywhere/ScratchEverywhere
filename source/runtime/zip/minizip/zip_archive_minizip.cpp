#include "zip_archive.hpp"

#include <cstdint>
#include <cstdlib>
#include <istream>
#include <memory>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

namespace {

struct IStreamZipStream {
    mz_stream base{};
    std::istream *stream = nullptr;
    int64_t size = 0;

    static int32_t openCb(void *, const char *, int32_t) { return MZ_OK; }

    static int32_t isOpenCb(void *s) {
        return static_cast<IStreamZipStream *>(s)->stream ? MZ_OK : MZ_STREAM_ERROR;
    }

    static int32_t readCb(void *s, void *buf, int32_t len) {
        auto *self = static_cast<IStreamZipStream *>(s);
        self->stream->clear();
        self->stream->read(static_cast<char *>(buf), len);
        return static_cast<int32_t>(self->stream->gcount());
    }

    static int32_t writeCb(void *, const void *, int32_t) { return MZ_STREAM_ERROR; }

    static int64_t tellCb(void *s) {
        auto *self = static_cast<IStreamZipStream *>(s);
        return static_cast<int64_t>(self->stream->tellg());
    }

    static int32_t seekCb(void *s, int64_t offset, int32_t origin) {
        auto *self = static_cast<IStreamZipStream *>(s);
        std::ios::seekdir dir = std::ios::beg;
        if (origin == MZ_SEEK_CUR) dir = std::ios::cur;
        else if (origin == MZ_SEEK_END) dir = std::ios::end;

        self->stream->clear();
        self->stream->seekg(offset, dir);
        return self->stream->good() ? MZ_OK : MZ_SEEK_ERROR;
    }

    static int32_t closeCb(void *) { return MZ_OK; }
    static int32_t errorCb(void *) { return MZ_OK; }

    static int32_t getPropInt64Cb(void *s, int32_t prop, int64_t *value) {
        auto *self = static_cast<IStreamZipStream *>(s);
        if (prop == MZ_STREAM_PROP_TOTAL_IN || prop == MZ_STREAM_PROP_DISK_SIZE) {
            *value = self->size;
            return MZ_OK;
        }
        return MZ_EXIST_ERROR;
    }
};

mz_stream_vtbl kIStreamVtbl = {
    IStreamZipStream::openCb,
    IStreamZipStream::isOpenCb,
    IStreamZipStream::readCb,
    IStreamZipStream::writeCb,
    IStreamZipStream::tellCb,
    IStreamZipStream::seekCb,
    IStreamZipStream::closeCb,
    IStreamZipStream::errorCb,
    nullptr,
    nullptr,
    IStreamZipStream::getPropInt64Cb,
    nullptr,
};

class MinizipZipArchive : public ZipArchive {
  public:
    MinizipZipArchive() : handle(mz_zip_reader_create()) {}

    ~MinizipZipArchive() override {
        if (handle) {
            if (opened) mz_zip_reader_close(handle);
            mz_zip_reader_delete(&handle);
        }
    }

    bool openFile(const std::string &path) override {
        opened = handle && mz_zip_reader_open_file(handle, path.c_str()) == MZ_OK;
        return opened;
    }

    bool openMemory(const void *data, size_t size) override {
        opened = handle && mz_zip_reader_open_buffer(handle, static_cast<const uint8_t *>(data),
                                                     static_cast<int32_t>(size), 0) == MZ_OK;
        return opened;
    }

    bool openStream(std::istream *stream, uint64_t size) override {
        if (!handle) return false;
        streamWrapper = std::make_unique<IStreamZipStream>();
        streamWrapper->base.vtbl = &kIStreamVtbl;
        streamWrapper->stream = stream;
        streamWrapper->size = static_cast<int64_t>(size);
        opened = mz_zip_reader_open(handle, streamWrapper.get()) == MZ_OK;
        return opened;
    }

    void *extractToHeap(const std::string &name, size_t *outSize) override {
        if (!opened) return nullptr;
        if (mz_zip_reader_locate_entry(handle, name.c_str(), 0) != MZ_OK) return nullptr;

        mz_zip_file *info = nullptr;
        if (mz_zip_reader_entry_get_info(handle, &info) != MZ_OK || !info) return nullptr;
        if (info->uncompressed_size < 0 || info->uncompressed_size > INT32_MAX) return nullptr;

        int32_t size = static_cast<int32_t>(info->uncompressed_size);
        void *buf = malloc(size > 0 ? static_cast<size_t>(size) : 1);
        if (!buf) return nullptr;

        if (size > 0 && mz_zip_reader_entry_save_buffer(handle, buf, size) != MZ_OK) {
            free(buf);
            return nullptr;
        }

        if (outSize) *outSize = static_cast<size_t>(size);
        return buf;
    }

    void freeHeap(void *ptr) override {
        free(ptr);
    }

    bool extractAll(const std::function<std::string(const std::string &)> &decide) override {
        if (!opened) return false;

        int32_t err = mz_zip_reader_goto_first_entry(handle);
        if (err == MZ_END_OF_LIST) return true;

        while (err == MZ_OK) {
            mz_zip_file *info = nullptr;
            if (mz_zip_reader_entry_get_info(handle, &info) != MZ_OK || !info) return false;

            std::string outPath = decide(info->filename);
            if (!outPath.empty()) {
                if (mz_zip_reader_entry_save_file(handle, outPath.c_str()) != MZ_OK) return false;
            }

            err = mz_zip_reader_goto_next_entry(handle);
        }
        return err == MZ_END_OF_LIST;
    }

  private:
    void *handle = nullptr;
    bool opened = false;
    std::unique_ptr<IStreamZipStream> streamWrapper;
};

} // namespace

std::unique_ptr<ZipArchive> createZipArchive() {
    return std::make_unique<MinizipZipArchive>();
}
