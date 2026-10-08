#ifndef _XBOX_SHLWAPI_H
#define _XBOX_SHLWAPI_H

#include <windows.h>
#include <stddef.h>

#ifdef __cplusplus
namespace std {
    static inline size_t mbstowcs(wchar_t *dest, const char *src, size_t n) { return 0; }
}
#endif

typedef struct _SHFILEOPSTRUCTW {
    HWND         hwnd;
    UINT         wFunc;
    const wchar_t* pFrom;
    const wchar_t* pTo;
    UINT         fFlags;
    BOOL         fAnyOperationsAborted;
    LPVOID       hNameMappings;
    const wchar_t* lpszProgressTitle;
} SHFILEOPSTRUCTW, *LPSHFILEOPSTRUCTW;

#define FO_DELETE 3
#define FOF_NO_UI 0
#define FOF_NOCONFIRMATION 0
#define FOF_NOERRORUI 0
#define FOF_SILENT 0

static inline int SHFileOperationW(LPSHFILEOPSTRUCTW lpFileOp) { return 0; }

typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    wchar_t cFileName[260];
    wchar_t cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

static inline HANDLE FindFirstFileW(const wchar_t* lpFileName, LPWIN32_FIND_DATAW lpFindFileData) { return (HANDLE)-1; }
static inline BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) { return 0; }

#define CP_UTF8 65001
static inline int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, const wchar_t* lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, PBOOL lpUsedDefaultChar) { return 0; }

#endif
