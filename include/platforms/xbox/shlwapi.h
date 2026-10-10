#pragma once
#ifndef _XBOX_SHLWAPI_H
#define _XBOX_SHLWAPI_H

#include <stddef.h>
#include <windows.h>

typedef struct _SHFILEOPSTRUCTW {
    HWND hwnd;
    UINT wFunc;
    const wchar_t *pFrom;
    const wchar_t *pTo;
    UINT fFlags;
    BOOL fAnyOperationsAborted;
    LPVOID hNameMappings;
    const wchar_t *lpszProgressTitle;
} SHFILEOPSTRUCTW, *LPSHFILEOPSTRUCTW;

#define FO_DELETE 3
#define FOF_NO_UI 0
#define FOF_NOCONFIRMATION 0
#define FOF_NOERRORUI 0
#define FOF_SILENT 0

static inline int SHFileOperationW(LPSHFILEOPSTRUCTW lpFileOp) { return 0; }

#endif
