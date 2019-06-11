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

#pragma once

#include "wslwrap.h"
#include "wslutils.h"

// NOTE: This is based on the work of LxRunOffline's WSL filesystem support

/* File type modes compatible with Linux */
#define LX_IFMT     0170000
#define LX_IFDIR    0040000
#define LX_IFCHR    0020000
#define LX_IFBLK    0060000
#define LX_IFREG    0100000
#define LX_IFIFO    0010000
#define LX_IFLNK    0120000
#define LX_IFSOCK   0140000

struct WslAttr
{
    uint16_t flags;
    uint16_t ver;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t rdev;
    uint32_t atime_nsec;
    uint32_t mtime_nsec;
    uint32_t ctime_nsec;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;

    WslAttr() { }   // Default uninitialized

    WslAttr(uint32_t mode_, uint32_t uid_, uint32_t gid_)
        : flags(), ver(1), mode(mode_), uid(uid_), gid(gid_), rdev(),
          atime_nsec(), mtime_nsec(), ctime_nsec(), atime(), mtime(),
          ctime() { }

    WslAttr(uint32_t mode_, uint32_t uid_, uint32_t gid_,
            uint64_t atime_, uint32_t atime_nsec_,
            uint64_t mtime_, uint32_t mtime_nsec_,
            uint64_t ctime_, uint32_t ctime_nsec_)
        : flags(), ver(1), mode(mode_), uid(uid_), gid(gid_), rdev(),
          atime_nsec(atime_nsec_), mtime_nsec(mtime_nsec_),
          ctime_nsec(ctime_nsec_), atime(atime_), mtime(mtime_),
          ctime(ctime_) { }
};

class WslFs
{
public:
    WslFs(const std::wstring &path);

    static WslFs create(const std::wstring &path);

    std::wstring rootPath() const { return m_rootPath; }
    WslApi::Version version() const { return m_version; }

    std::wstring path(const std::string_view &unixPath) const;

    WslAttr getAttr(HANDLE hFile) const;
    void setAttr(HANDLE hFile, const WslAttr &attr) const;

    UniqueHandle createFile(const std::string_view &unixPath, const WslAttr &attr) const;
    UniqueHandle openFile(const std::string_view &unixPath) const;

    bool createDirectory(const std::string_view &unixPath, const WslAttr &attr) const;
    bool createSymlink(const std::string_view &unixPath,
                       const std::string_view &target, const WslAttr &attr) const;
    bool createHardLink(const std::string_view &unixPath,
                        const std::string_view &unixTarget) const;

private:
    WslApi::Version m_version;
    std::wstring m_rootPath;

    WslFs(WslApi::Version version, const std::wstring &path);
};
