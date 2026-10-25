#pragma once
#include <se_export.hpp>

#include <iosfwd>
#include <memory>
#include <optional>
#include <os.hpp>
#include <parser.hpp>
#include <project_loader.hpp>
#include <string>
#include <vector>
#include <zip_archive.hpp>

#ifdef ENABLE_CLOUDVARS
extern std::string projectJSON;
#endif

#ifdef __XBOX__
#include <type_traits>
#include <utility>

// Allocator whose default construction leaves trivial types uninitialized, so vector::resize()
// doesn't zero-fill memory that is about to be overwritten by a file read.
template <typename T, typename A = std::allocator<T>>
class DefaultInitAllocator : public A {
    using traits = std::allocator_traits<A>;

  public:
    template <typename U>
    struct rebind {
        using other = DefaultInitAllocator<U, typename traits::template rebind_alloc<U>>;
    };
    using A::A;
    template <typename U>
    void construct(U *ptr) noexcept(std::is_nothrow_default_constructible<U>::value) {
        ::new (static_cast<void *>(ptr)) U;
    }
    template <typename U, typename... Args>
    void construct(U *ptr, Args &&...args) {
        traits::construct(static_cast<A &>(*this), ptr, std::forward<Args>(args)...);
    }
};
#endif

class SE_EXPORT Unzip {
  public:
    static volatile int projectOpened;
    static std::string loadingState;
#ifdef __XBOX__
    static void setLoadingState(const std::string &state);
    static std::string getLoadingState();
#else
    static inline void setLoadingState(const std::string &state) { loadingState = state; }
    static inline const std::string &getLoadingState() { return loadingState; }
#endif
    static volatile bool threadFinished;
    static std::string filePath;
    static bool UnpackedInSD;
    static std::unique_ptr<ZipArchive> zipArchive;
#ifdef __XBOX__
    using ZipBuffer = std::vector<char, DefaultInitAllocator<char>>;
#else
    using ZipBuffer = std::vector<char>;
#endif
    static ZipBuffer zipBuffer;
    static std::unique_ptr<ProjectLoader> loader;

    static std::optional<ProjectFormat> unpackedFormatHint;

    static void openScratchProject(void *arg);
    static std::vector<std::string> getProjectFiles(const std::string &directory);
    static void *getFileInSB3(const std::string &fileName, size_t *outSize = nullptr);
    static int openFile(std::istream *&file, ProjectFormat format);
    static bool load();
    static bool extractProject(const std::string &zipPath, const std::string &destFolder);
    static bool deleteProjectFolder(const std::string &directory);
    static nlohmann::json getSetting(const std::string &settingName);

    static ProjectFormat detectFormat(const std::string &path);

    static std::string resolveZipProjectPath(const std::string &baseNoExt, ProjectFormat &format);
};
