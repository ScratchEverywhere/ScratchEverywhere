#ifndef _XBOX_WINDOWS_H
#define _XBOX_WINDOWS_H

#include_next <windows.h>

static inline size_t mbstowcs(wchar_t *dest, const char *src, size_t n) {
    if (!src) return 0;
    if (!dest) {
        size_t len = 0;
        while (src[len]) len++;
        return len;
    }
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dest[i] = (wchar_t)(unsigned char)src[i];
    }
    if (i < n) dest[i] = 0;
    return i;
}

#ifdef __cplusplus
namespace std {
    using ::mbstowcs;
}
#endif

typedef struct _SHFILEOPSTRUCTW {
    HWND         hwnd;
    UINT         wFunc;
    LPCWSTR      pFrom;
    LPCWSTR      pTo;
    UINT         fFlags;
    BOOL         fAnyOperationsAborted;
    LPVOID       hNameMappings;
    LPCWSTR      lpszProgressTitle;
} SHFILEOPSTRUCTW, *LPSHFILEOPSTRUCTW;

#define FO_DELETE 3
#define FOF_NO_UI 0
#define FOF_NOCONFIRMATION 0
#define FOF_NOERRORUI 0
#define FOF_SILENT 0

static inline int SHFileOperationW(LPSHFILEOPSTRUCTW lpFileOp) { return 0; }

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR cFileName[260];
    WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

static inline HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
    if (!lpFileName) return (HANDLE)-1;
    char aFileName[260];
    int i = 0;
    while (lpFileName[i] && i < 259) {
        aFileName[i] = (char)lpFileName[i];
        i++;
    }
    aFileName[i] = '\0';
    WIN32_FIND_DATAA aData;
    HANDLE h = FindFirstFileA(aFileName, &aData);
    if (h != (HANDLE)-1 && lpFindFileData) {
        lpFindFileData->dwFileAttributes = aData.dwFileAttributes;
        lpFindFileData->ftCreationTime = aData.ftCreationTime;
        lpFindFileData->ftLastAccessTime = aData.ftLastAccessTime;
        lpFindFileData->ftLastWriteTime = aData.ftLastWriteTime;
        lpFindFileData->nFileSizeHigh = aData.nFileSizeHigh;
        lpFindFileData->nFileSizeLow = aData.nFileSizeLow;
        int j = 0;
        while (aData.cFileName[j] && j < 259) {
            lpFindFileData->cFileName[j] = (WCHAR)aData.cFileName[j];
            j++;
        }
        lpFindFileData->cFileName[j] = 0;
    }
    return h;
}

static inline BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {
    WIN32_FIND_DATAA aData;
    BOOL res = FindNextFileA(hFindFile, &aData);
    if (res && lpFindFileData) {
        lpFindFileData->dwFileAttributes = aData.dwFileAttributes;
        lpFindFileData->ftCreationTime = aData.ftCreationTime;
        lpFindFileData->ftLastAccessTime = aData.ftLastAccessTime;
        lpFindFileData->ftLastWriteTime = aData.ftLastWriteTime;
        lpFindFileData->nFileSizeHigh = aData.nFileSizeHigh;
        lpFindFileData->nFileSizeLow = aData.nFileSizeLow;
        int j = 0;
        while (aData.cFileName[j] && j < 259) {
            lpFindFileData->cFileName[j] = (WCHAR)aData.cFileName[j];
            j++;
        }
        lpFindFileData->cFileName[j] = 0;
    }
    return res;
}

#define CP_UTF8 65001
static inline int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar) {
    if (!lpWideCharStr) return 0;
    int len = 0;
    if (cchWideChar < 0) {
        while (lpWideCharStr[len]) len++;
        len++; // include null terminator
    } else {
        len = cchWideChar;
    }
    if (cbMultiByte == 0 || !lpMultiByteStr) {
        return len;
    }
    int toCopy = (len < cbMultiByte) ? len : cbMultiByte;
    for (int i = 0; i < toCopy; i++) {
        lpMultiByteStr[i] = (char)lpWideCharStr[i];
    }
    return toCopy;
}

#endif
