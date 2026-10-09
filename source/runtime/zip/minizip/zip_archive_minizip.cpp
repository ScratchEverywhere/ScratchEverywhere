#include "zip_archive.hpp"
#include <log.hpp>

#include <cstdint>
#include <cstdlib>
#include <istream>
#include <memory>
#include <string>
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
        int32_t err = handle ? mz_zip_reader_open_file(handle, path.c_str()) : MZ_PARAM_ERROR;
        if (err != MZ_OK) {
            Log::logError("mz_zip_reader_open_file failed for " + path + " err=" + std::to_string(err));
        }
        opened = (err == MZ_OK);
        return opened;
    }

    bool openMemory(const void *data, size_t size) override {
        int32_t err = handle ? mz_zip_reader_open_buffer(handle, static_cast<uint8_t *>(const_cast<void *>(data)),
                                                         static_cast<int32_t>(size), 0) : MZ_PARAM_ERROR;
        if (err != MZ_OK) {
            Log::logError("mz_zip_reader_open_buffer failed err=" + std::to_string(err) + " size=" + std::to_string(size));
        }
        opened = (err == MZ_OK);
        return opened;
    }

    bool openStream(std::istream *stream, uint64_t size) override {
        if (!handle) return false;
        streamWrapper = std::make_unique<IStreamZipStream>();
        streamWrapper->base.vtbl = &kIStreamVtbl;
        streamWrapper->stream = stream;
        streamWrapper->size = static_cast<int64_t>(size);
        int32_t err = mz_zip_reader_open(handle, streamWrapper.get());
        if (err != MZ_OK) {
            Log::logError("mz_zip_reader_open stream failed err=" + std::to_string(err));
        }
        opened = (err == MZ_OK);
        return opened;
    }

    void *extractToHeap(const std::string &name, size_t *outSize) override {
        if (!opened) {
            Log::logError("MinizipZipArchive::extractToHeap: archive not open!");
            return nullptr;
        }

        int32_t err = mz_zip_reader_locate_entry(handle, name.c_str(), 1);
        if (err != MZ_OK) {
            err = mz_zip_reader_locate_entry(handle, ("./" + name).c_str(), 1);
        }
        if (err != MZ_OK) {
            err = mz_zip_reader_goto_first_entry(handle);
            while (err == MZ_OK) {
                mz_zip_file *file_info = nullptr;
                if (mz_zip_reader_entry_get_info(handle, &file_info) == MZ_OK && file_info && file_info->filename) {
                    std::string filename = file_info->filename;
                    if (filename == name ||
                        (filename.size() >= name.size() &&
                         filename.compare(filename.size() - name.size(), name.size(), name) == 0)) {
                        break;
                    }
                }
                err = mz_zip_reader_goto_next_entry(handle);
            }
        }

        if (err != MZ_OK) {
            Log::logError("MinizipZipArchive::extractToHeap: locate entry failed for " + name + " err=" + std::to_string(err));
            return nullptr;
        }

        mz_zip_file *info = nullptr;
        if (mz_zip_reader_entry_get_info(handle, &info) != MZ_OK || !info) {
            Log::logError("MinizipZipArchive::extractToHeap: entry_get_info failed for " + name);
            return nullptr;
        }
        if (info->uncompressed_size < 0 || info->uncompressed_size > INT32_MAX) {
            Log::logError("MinizipZipArchive::extractToHeap: uncompressed_size invalid for " + name);
            return nullptr;
        }

        int32_t size = static_cast<int32_t>(info->uncompressed_size);
        void *buf = malloc(size > 0 ? static_cast<size_t>(size) : 1);
        if (!buf) {
            Log::logError("MinizipZipArchive::extractToHeap: malloc failed size=" + std::to_string(size));
            return nullptr;
        }

        if (size > 0) {
            int32_t save_err = mz_zip_reader_entry_save_buffer(handle, buf, size);
            if (save_err != MZ_OK) {
                Log::logError("mz_zip_reader_entry_save_buffer failed err=" + std::to_string(save_err) + " trying entry_open+read");
                int32_t open_err = mz_zip_reader_entry_open(handle);
                if (open_err == MZ_OK) {
                    int32_t read_bytes = mz_zip_reader_entry_read(handle, buf, size);
                    mz_zip_reader_entry_close(handle);
                    if (read_bytes < 0) {
                        Log::logError("mz_zip_reader_entry_read failed ret=" + std::to_string(read_bytes));
                        free(buf);
                        return nullptr;
                    }
                } else {
                    Log::logError("mz_zip_reader_entry_open failed err=" + std::to_string(open_err));
                    free(buf);
                    return nullptr;
                }
            }
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
