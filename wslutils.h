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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QString>
#include <string>
#include <atomic>

class QWidget;
class QIcon;

#define INVALID_UID 0xffffffff

struct WslConsoleContext
{
    std::wstring distName;
    std::atomic<bool> shellTerminated = false;
    QString errorMessage;
    HICON distIconBig = nullptr;
    HICON distIconSmall = nullptr;

    std::atomic<int> refs = 1;
    void ref() { ++refs; }
    void unref()
    {
        if (--refs == 0)
            delete this;
    }

    ~WslConsoleContext()
    {
        DestroyIcon(distIconBig);
        DestroyIcon(distIconSmall);
    }

    static WslConsoleContext *createConsole(const std::wstring &name, const QIcon &icon);

    void startConsoleThread(QWidget *parent, unsigned (*proc)(void *));
};

class UniqueHandle
{
public:
    UniqueHandle() noexcept : m_handle() { }
    UniqueHandle(std::nullptr_t) noexcept : m_handle() { }
    UniqueHandle(HANDLE h) noexcept : m_handle(h) { }

    UniqueHandle(UniqueHandle &&uh) noexcept
        : m_handle(uh.m_handle)
    {
        uh.m_handle = nullptr;
    }

    bool isValid() const noexcept
    {
        return (m_handle && m_handle != INVALID_HANDLE_VALUE);
    }

    UniqueHandle &operator=(std::nullptr_t) noexcept
    {
        release();
        return *this;
    }

    UniqueHandle &operator=(UniqueHandle &&uh) noexcept
    {
        if (isValid())
            CloseHandle(m_handle);
        m_handle = uh.m_handle;
        uh.m_handle = nullptr;
        return *this;
    }

    UniqueHandle(const UniqueHandle &) = delete;
    UniqueHandle &operator=(const UniqueHandle &) = delete;

    ~UniqueHandle() noexcept
    {
        if (isValid())
            CloseHandle(m_handle);
    }

    void release() noexcept
    {
        if (isValid()) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

    HANDLE get() const noexcept { return m_handle; }

    // Unlike unique_ptr, we can use this to receive a handle from an API call...
    HANDLE *receive() noexcept
    {
        release();
        return &m_handle;
    }

    bool operator==(HANDLE h) const noexcept { return m_handle == h; }
    bool operator!=(HANDLE h) const noexcept { return m_handle != h; }
    bool operator==(UniqueHandle uh) const noexcept { return m_handle == uh.m_handle; }
    bool operator!=(UniqueHandle uh) const noexcept { return m_handle != uh.m_handle; }

private:
    HANDLE m_handle;
};

namespace WslUtil
{
    QString getUsername(const std::wstring &distName, uint32_t uid);
    uint32_t getUid(const std::wstring &distName, const QString &username);

    std::wstring fromUtf8(const std::string_view &utf8);
    std::string toUtf8(const std::wstring_view &wide);
}

// TODO: Use C++20
inline bool starts_with(const std::wstring_view &str, const wchar_t *prefix)
{
    return str.rfind(prefix, 0) == 0;
}

inline bool starts_with(const std::wstring_view &str, const std::wstring &prefix)
{
    return str.rfind(prefix, 0) == 0;
}
