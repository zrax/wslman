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

#define WIN32_NO_STATUS     // Conflicts with ntstatus.h
#include "wslfs.h"

#include <aclapi.h>
#include <winternl.h>
#include <winioctl.h>

#undef WIN32_NO_STATUS
#include <ntstatus.h>

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

#define IO_REPARSE_TAG_LX_SYMLINK       0xA000001DL
#define FILE_CS_FLAG_CASE_SENSITIVE_DIR 0x00000001
#define FileCaseSensitiveInformation    static_cast<FILE_INFORMATION_CLASS>(71)

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

struct REPARSE_DATA_BUFFER
{
    ULONG   ReparseTag;
    USHORT  ReparseDataLength;
    USHORT  Reserved;
    UCHAR   DataBuffer[1];
};

struct FILE_CASE_SENSITIVE_INFORMATION
{
    ULONG Flags;
};

class InvalidAttribute : public std::runtime_error
{
public:
    InvalidAttribute() : std::runtime_error("Invalid NT Extended Attribute value") { }
};

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
        return WslFs::InvalidFormat;

    try {
        (void)getNtExAttr<uint32_t>(hFile.get(), "$LXUID");
        return WslFs::WslFsFormat;
    } catch (const InvalidAttribute &) {
        try {
            (void)getNtExAttr<WslAttr>(hFile.get(), "LXATTRB");
            return WslFs::LxFsFormat;
        } catch (const InvalidAttribute &) {
            return WslFs::InvalidFormat;
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

WslFs::WslFs(Format format, const std::wstring &path)
    : m_format(format)
{
    if (starts_with(path, LR"(\\?\)"))
        m_rootPath = path;
    else
        m_rootPath = LR"(\\?\)" + path;
}

WslFs WslFs::create(const std::wstring &path)
{
    Format format = WslUtil::checkWindowsVersion(WslUtil::Windows1809)
                  ? WslFsFormat : LxFsFormat;
    WslFs rootfs(format, path);

    LARGE_INTEGER sysTime;
    NtQuerySystemTime(&sysTime);
    uint32_t unixTimeNsec;
    uint64_t unixTime = fileTimeToUnix(sysTime, unixTimeNsec);

    const WslAttr attr(0040755, 0, 0, unixTime, unixTimeNsec,
                       unixTime, unixTimeNsec, unixTime, unixTimeNsec);
    if (!rootfs.createDirectory("", attr))
        throw std::runtime_error("Could not initialize root directory");

    return rootfs;
}

static std::wstring encodeChar(WslFs::Format format, wchar_t ch)
{
    switch (format) {
    case WslFs::LxFsFormat:
        {
            wchar_t buffer[8];
            swprintf(buffer, std::size(buffer), L"#%04X", ch);
            return buffer;
        }
    case WslFs::WslFsFormat:
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

std::wstring WslFs::path(const std::string_view &unixPath) const
{
    std::wstring unixWide = WslUtil::fromUtf8(unixPath);

    std::wstring result;
    result.reserve(m_rootPath.size() + unixWide.size() + 1);
    result.append(m_rootPath);
    if (result.back() != L'\\')
        result.push_back(L'\\');
    if (!unixWide.empty()) {
        if (unixWide.front() == L'/')
            result.append(encodePath(m_format, std::wstring_view(unixWide).substr(1)));
        else
            result.append(encodePath(m_format, unixWide));
    }
    return result;
}

WslAttr WslFs::getAttr(HANDLE hFile) const
{
    switch (m_format) {
    case LxFsFormat:
        return getNtExAttr<WslAttr>(hFile, "LXATTRB");
    case WslFsFormat:
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
    case LxFsFormat:
        setNtExAttr(hFile, "LXATTRB", attr);
        break;
    case WslFsFormat:
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

UniqueHandle WslFs::createFile(const std::string_view &unixPath, const WslAttr &attr) const
{
    const uint32_t ftype = attr.mode & LX_IFMT;
    if (ftype == 0 || ftype == LX_IFDIR)
        throw std::invalid_argument("Invalid file mode");

    UniqueHandle hFile = CreateFileW(path(unixPath).c_str(), MAXIMUM_ALLOWED,
                                     FILE_SHARE_READ, nullptr, CREATE_NEW,
                                     FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
        setAttr(hFile.get(), attr);

    return hFile;
}

UniqueHandle WslFs::openFile(const std::string_view &unixPath) const
{
    return CreateFileW(path(unixPath).c_str(), MAXIMUM_ALLOWED,
                       FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}

bool WslFs::createDirectory(const std::string_view &unixPath, const WslAttr &attr) const
{
    if ((attr.mode & LX_IFMT) != LX_IFDIR)
        throw std::invalid_argument("Invalid directory mode");

    std::wstring wslPath = path(unixPath);
    if (!CreateDirectoryW(wslPath.c_str(), nullptr)) {
        auto err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
            return false;
    }

    UniqueHandle hDir = CreateFileW(path(unixPath).c_str(), MAXIMUM_ALLOWED,
                                    FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (hDir == INVALID_HANDLE_VALUE)
        return false;

    setAttr(hDir.get(), attr);

    // Case sensitivity is set on a per-directory basis
    FILE_CASE_SENSITIVE_INFORMATION info = {FILE_CS_FLAG_CASE_SENSITIVE_DIR};
    IO_STATUS_BLOCK iosb;
    memset(&iosb, 0, sizeof(iosb));
    auto rc = NtSetInformationFile(hDir.get(), &iosb, &info, sizeof(info),
                                   FileCaseSensitiveInformation);
    if (rc == STATUS_ACCESS_DENIED) {
        // Directories which already exist might require FILE_DELETE_CHILD
        // permission in order to modify existing files already in the directory
        ACL *acl;
        PSECURITY_DESCRIPTOR security;
        if (GetSecurityInfo(hDir.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                            nullptr, nullptr, &acl, nullptr, &security) != ERROR_SUCCESS)
            return false;

        EXPLICIT_ACCESSW access;
        ACL *newAcl;
        BuildExplicitAccessWithNameW(&access, const_cast<wchar_t *>(L"CURRENT_USER"),
                                     FILE_DELETE_CHILD, GRANT_ACCESS, CONTAINER_INHERIT_ACE);
        if (SetEntriesInAclW(1, &access, acl, &newAcl) != ERROR_SUCCESS) {
            LocalFree(security);
            return false;
        }

        SetSecurityInfo(hDir.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                        nullptr, nullptr, newAcl, nullptr);
        LocalFree(newAcl);
        LocalFree(security);

        // Try again
        rc = NtSetInformationFile(hDir.get(), &iosb, &info, sizeof(info),
                                  FileCaseSensitiveInformation);
    }
    return rc == 0;
}

bool WslFs::createSymlink(const std::string_view &unixPath,
                          const std::string_view &target, const WslAttr &attr) const
{
    if ((attr.mode & LX_IFMT) != LX_IFLNK)
        throw std::invalid_argument("Invalid symlink file mode");

    UniqueHandle hFile = createFile(unixPath, attr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    switch (m_format) {
    case LxFsFormat:
        {
            DWORD nWritten;
            return WriteFile(hFile.get(), target.data(), static_cast<DWORD>(target.size()),
                             &nWritten, nullptr);
        }
    case WslFsFormat:
        {
            size_t dataLen = sizeof(uint32_t) + target.size();
            auto bufLen = static_cast<DWORD>(offsetof(REPARSE_DATA_BUFFER, DataBuffer) + dataLen);
            auto buffer = std::make_unique<std::byte[]>(bufLen);
            auto reparse = reinterpret_cast<REPARSE_DATA_BUFFER *>(buffer.get());
            reparse->ReparseTag = IO_REPARSE_TAG_LX_SYMLINK;
            reparse->ReparseDataLength = static_cast<USHORT>(dataLen);
            reparse->Reserved = 0;
            uint32_t data = 2;      // TODO: What is this?
            memcpy(reparse->DataBuffer, &data, sizeof(data));
            memcpy(reparse->DataBuffer + sizeof(data), target.data(), target.size());

            DWORD nReturned;
            return DeviceIoControl(hFile.get(), FSCTL_SET_REPARSE_POINT, reparse,
                                   bufLen, nullptr, 0, &nReturned, nullptr);
        }
    default:
        throw std::runtime_error("Cannot create a symlink in an invalid WSL Root");
    }
}

bool WslFs::createHardLink(const std::string_view &unixPath,
                           const std::string_view &unixTarget) const
{
    std::wstring winPath = path(unixPath);
    std::wstring winTarget = path(unixTarget);
    return CreateHardLinkW(winPath.c_str(), winTarget.c_str(), nullptr);
}
