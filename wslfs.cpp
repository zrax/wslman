/* This file is part of wslman.
 *
 * wslman is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * wslman is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wslman.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "wslfs.h"

#include <winternl.h>

// NOTE: This is based on the work of LxRunOffline's WSL filesystem support

extern "C" {
    NTSYSAPI NTSTATUS NTAPI NtQueryEaFile(
        _In_ HANDLE FileHandle,
        _Out_ PIO_STATUS_BLOCK IoStatusBlock,
        _Out_ PVOID Buffer,
        _In_ ULONG Length,
        _In_ BOOLEAN ReturnSingleEntry,
        _In_opt_ PVOID EaList,
        _In_ ULONG EaListLength,
        _In_opt_ PULONG EaIndex,
        _In_ BOOLEAN RestartScan
    );

    NTSYSAPI NTSTATUS NTAPI NtSetEaFile(
        _In_ HANDLE FileHandle,
        _Out_ PIO_STATUS_BLOCK IoStatusBlock,
        _In_ PVOID EaBuffer,
        _In_ ULONG EaBufferSize
    );

    NTSYSAPI NTSTATUS NTAPI NtQueryInformationFile(
        _In_ HANDLE FileHandle,
        _Out_ PIO_STATUS_BLOCK IoStatusBlock,
        _Out_ PVOID FileInformation,
        _In_ ULONG Length,
        _In_ FILE_INFORMATION_CLASS FileInformationClass
    );

    NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(
        _In_ HANDLE FileHandle,
        _Out_ PIO_STATUS_BLOCK IoStatusBlock,
        _In_ PVOID FileInformation,
        _In_ ULONG Length,
        _In_ FILE_INFORMATION_CLASS FileInformationClass
    );
}

template <size_t NameLength>
struct FILE_GET_EA_INFORMATION
{
    ULONG   NextEntryOffset = 0;
    UCHAR   EaNameLength = NameLength - 1;
    CHAR    EaName[NameLength];

    FILE_GET_EA_INFORMATION(const char (&name)[NameLength])
    {
        memcpy(EaName, name, NameLength);
    }
};

template <size_t NameLength, typename AttrType>
struct FILE_FULL_EA_INFORMATION
{
    ULONG   NextEntryOffset = 0;
    UCHAR   Flags = 0;
    UCHAR   EaNameLength = NameLength - 1;
    USHORT  EaValueLength = sizeof(AttrType);
    CHAR    EaName[NameLength];
    CHAR    AttrBuffer[sizeof(AttrType)];

    FILE_FULL_EA_INFORMATION(const char (&name)[NameLength])
    {
        memcpy(EaName, name, NameLength);
        memset(AttrBuffer, 0, sizeof(AttrType));
    }

    AttrType getAttr() const
    {
        AttrType value;
        memcpy(&value, AttrBuffer, sizeof(AttrType));
        return value;
    }

    void setAttr(const AttrType &value)
    {
        memcpy(AttrBuffer, &value, sizeof(AttrType));
    }
};

class InvalidAttribute : public std::runtime_error
{
public:
    InvalidAttribute() : std::runtime_error("Invalid NT Extended Attribute value") { }
};

static QString ntdllError(const QString &prefix, HRESULT rc)
{
    wchar_t buffer[512];
    swprintf(buffer, std::size(buffer), L"0x%08x", rc);

    HMODULE hmNtDll = LoadLibraryW(L"ntdll.dll");
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
                   hmNtDll, rc, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buffer, static_cast<DWORD>(std::size(buffer)), nullptr);
    FreeLibrary(hmNtDll);

    return QStringLiteral("%1: %2").arg(prefix).arg(QString::fromWCharArray(buffer));
}

template <typename AttrType, size_t NameLength>
AttrType getNtExAttr(HANDLE hFile, const char (&name)[NameLength])
{
    IO_STATUS_BLOCK iosb;
    memset(&iosb, 0, sizeof(iosb));

    FILE_GET_EA_INFORMATION<NameLength> getInfo(name);
    FILE_FULL_EA_INFORMATION<NameLength, AttrType> eaInfo(name);

    auto rc = NtQueryEaFile(hFile, &iosb, &eaInfo, sizeof(eaInfo), TRUE,
                            &getInfo, sizeof(getInfo), nullptr, TRUE);

    if (rc != 0) {
        QString error = ntdllError("Failed to query NT Extended Attribute", rc);
        throw std::runtime_error(error.toStdString());
    }
    if (eaInfo.EaValueLength != sizeof(AttrType))
        throw InvalidAttribute();

    return eaInfo.getAttr();
}

template <typename AttrType, size_t NameLength>
void setNtExAttr(HANDLE hFile, const char (&name)[NameLength], const AttrType &value)
{
    IO_STATUS_BLOCK iosb;
    memset(&iosb, 0, sizeof(iosb));

    FILE_FULL_EA_INFORMATION<NameLength, AttrType> eaInfo(name);
    eaInfo.setAttr(value);

    auto rc = NtSetEaFile(hFile, &iosb, &eaInfo, sizeof(eaInfo));
    if (rc != 0) {
        QString error = ntdllError("Failed to set NT Extended Attribute", rc);
        throw std::runtime_error(error.toStdString());
    }
}

static WslFs::Format detectFormat(const std::wstring &path)
{
    UniqueHandle hFile = CreateFileW(path.c_str(), MAXIMUM_ALLOWED,
                                     FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                     FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return WslFs::Invalid;

    try {
        (void)getNtExAttr<uint32_t>(hFile.get(), "$LXUID");
        return WslFs::WSLv2;
    } catch (const InvalidAttribute &) {
        try {
            (void)getNtExAttr<WslAttr>(hFile.get(), "LXATTRB");
            return WslFs::WSLv1;
        } catch (const InvalidAttribute &) {
            return WslFs::Invalid;
        }
    }
}

WslFs::WslFs(const std::wstring &path)
{
    if (starts_with(path, LR"(\\?\)"))
        m_rootPath = path;
    else
        m_rootPath = LR"(\\?\)" + path;

    m_format = detectFormat(m_rootPath);
}

static std::wstring encodeChar(WslFs::Format format, wchar_t ch)
{
    switch (format) {
    case WslFs::WSLv1:
        {
            wchar_t buffer[8];
            swprintf(buffer, std::size(buffer), L"#%04X", ch);
            return buffer;
        }
    case WslFs::WSLv2:
        return std::wstring(1, ch | 0xf000);
    default:
        return std::wstring(1, ch);
    }
}

static std::wstring encodePath(WslFs::Format format, const std::wstring_view &path)
{
    std::wstring result;
    result.reserve(path.size());
    for (wchar_t ch : path) {
        if (ch == L'/') {
            result.push_back(L'\\');
        } else if ((ch >= L'\x01' && ch <= L'\x1f') || ch == L'<' || ch == L'>'
                    || ch == L':' || ch == L'"' || ch == L'\\' || ch == L'|'
                    || ch == L'*' || ch == L'#') {
            result.append(encodeChar(format, ch));
        } else {
            result.push_back(ch);
        }
    }

    return result;
}

std::wstring WslFs::path(const std::wstring_view &unixPath) const
{
    std::wstring result;
    result.reserve(m_rootPath.size() + unixPath.size() + 1);
    result.append(m_rootPath);
    if (result.back() != L'\\')
        result.push_back(L'\\');
    if (!unixPath.empty()) {
        if (unixPath.front() == L'/')
            result.append(encodePath(m_format, unixPath.substr(1)));
        else
            result.append(encodePath(m_format, unixPath));
    }
    return result;
}

// FILETIME is 100-nanosecond intervals since January 1, 1601 (UTC)
#define FILETIME_OFFSET 116444736000000000

static LARGE_INTEGER unixToFileTime(uint64_t time, uint32_t nsec)
{
    LARGE_INTEGER fileTime;
    fileTime.QuadPart = (time * 10000000) + (nsec / 100) + FILETIME_OFFSET;
    return fileTime;
}

static uint64_t fileTimeToUnix(const LARGE_INTEGER &fileTime, uint32_t &nsec)
{
    auto divmod = std::lldiv(fileTime.QuadPart - FILETIME_OFFSET, 10000000);
    nsec = divmod.rem * 100;
    return divmod.quot;
}

WslAttr WslFs::getAttr(HANDLE hFile) const
{
    switch (m_format) {
    case WSLv1:
        return getNtExAttr<WslAttr>(hFile, "LXATTRB");
    case WSLv2:
        {
            WslAttr attr;
            attr.uid = getNtExAttr<uint32_t>(hFile, "$LXUID");
            attr.gid = getNtExAttr<uint32_t>(hFile, "$LXGID");
            attr.mode = getNtExAttr<uint32_t>(hFile, "$LXMOD");
            FILE_BASIC_INFO info;
            if (!GetFileInformationByHandleEx(hFile, FileBasicInfo, &info, sizeof(info)))
                throw std::runtime_error("Failed to query file extended info");
            attr.ctime = fileTimeToUnix(info.ChangeTime, attr.ctime_nsec);
            attr.atime = fileTimeToUnix(info.LastAccessTime, attr.atime_nsec);
            attr.mtime = fileTimeToUnix(info.LastWriteTime, attr.mtime_nsec);
            return attr;
        }
    default:
        throw std::runtime_error("Invalid file format");
    }
}

void WslFs::setAttr(HANDLE hFile, const WslAttr &attr) const
{
    switch (m_format) {
    case WSLv1:
        setNtExAttr(hFile, "LXATTRB", attr);
        break;
    case WSLv2:
        {
            setNtExAttr(hFile, "$LXUID", attr.uid);
            setNtExAttr(hFile, "$LXGID", attr.gid);
            setNtExAttr(hFile, "$LXMOD", attr.mode);
            FILE_BASIC_INFO info;
            info.CreationTime = unixToFileTime(attr.ctime, attr.ctime_nsec);
            info.LastAccessTime = unixToFileTime(attr.atime, attr.atime_nsec);
            info.LastWriteTime = unixToFileTime(attr.mtime, attr.mtime_nsec);
            info.ChangeTime = info.CreationTime;
            info.FileAttributes = 0;
            if (!SetFileInformationByHandle(hFile, FileBasicInfo, &info, sizeof(info)))
                throw std::runtime_error("Failed to set file extended info");
        }
        break;
    default:
        throw std::runtime_error("Invalid file format");
    }
}

UniqueHandle WslFs::createFile(const std::wstring &unixPath, const WslAttr &attr) const
{
    UniqueHandle hFile = CreateFileW(path(unixPath).c_str(), MAXIMUM_ALLOWED,
                                     FILE_SHARE_READ, nullptr, CREATE_NEW,
                                     FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
        setAttr(hFile.get(), attr);

    return hFile;
}

UniqueHandle WslFs::openFile(const std::wstring &unixPath) const
{
    return CreateFileW(path(unixPath).c_str(), MAXIMUM_ALLOWED,
                       FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}
