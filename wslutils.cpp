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

#include "wslutils.h"

#include "wslregistry.h"
#include "wslfs.h"
#include <QMessageBox>
#include <QIcon>
#include <QtWin>
#include <process.h>

WslConsoleContext *WslConsoleContext::createConsole(const std::wstring &name,
                                                    const QIcon &icon)
{
    SetConsoleTitleW(name.c_str());

    auto context = new WslConsoleContext;
    context->distName = name;

    const QIcon distIcon = icon;
    context->distIconBig = QtWin::toHICON(distIcon.pixmap(32, 32));
    context->distIconSmall = QtWin::toHICON(distIcon.pixmap(16, 16));
    HWND hConsole = GetConsoleWindow();
    SendMessageW(hConsole, WM_SETICON, ICON_BIG, (LPARAM)context->distIconBig);
    SendMessageW(hConsole, WM_SETICON, ICON_SMALL, (LPARAM)context->distIconSmall);

    return context;
}

void WslConsoleContext::startConsoleThread(QWidget *parent, unsigned (*proc)(void *))
{
    // Add a ref for the thread.  When the thread exits, it should call unref()
    ref();

    unsigned threadId;
    HANDLE th = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0,
                        proc, reinterpret_cast<void *>(this), 0, &threadId));
    CloseHandle(th);

    // Detach from the console once the WSL process starts using it
    DWORD processList[4];
    DWORD processCount = 4;
    for ( ;; ) {
        DWORD activeProcs = GetConsoleProcessList(processList, processCount);
        if (activeProcs > 1 || shellTerminated)
            break;
        Sleep(100);
    }
    if (!errorMessage.isEmpty())
        QMessageBox::critical(parent, QString::null, errorMessage);
    unref();
}

typedef std::tuple<std::string, uint32_t, uint32_t> UserInfo;

static std::list<UserInfo> readPasswdFile(const std::wstring &distName)
{
    WslRegistry registry;
    WslDistribution dist = registry.findDistByName(distName);
    if (!dist.isValid())
        return {};

    UniqueHandle hPasswd;
    try {
        WslFs rootfs(dist.rootfsPath());
        hPasswd = rootfs.openFile(L"/etc/passwd");
        if (hPasswd == INVALID_HANDLE_VALUE)
            return {};
    } catch (const std::runtime_error &) {
        return {};
    }

    std::string content;
    for ( ;; ) {
        char buffer[1024];
        DWORD nRead = 0;
        if (!ReadFile(hPasswd.get(), buffer, static_cast<DWORD>(std::size(buffer)),
                      &nRead, nullptr) || nRead == 0) {
            break;
        }
        content += std::string_view(buffer, nRead);
    }

    std::list<UserInfo> result;
    size_t start = 0;
    for ( ;; ) {
        size_t end = content.find(':', start);
        if (end == content.npos)
            break;
        std::string_view username(content.c_str() + start, end - start);
        start = end + 1;

        // Password hash
        end = content.find(':', start);
        if (end == content.npos)
            break;
        start = end + 1;

        // UID
        uint32_t uid = static_cast<uint32_t>(std::strtoul(content.c_str() + start, nullptr, 10));
        end = content.find(':', start);
        if (end == content.npos)
            break;
        start = end + 1;

        // GID
        uint32_t gid = static_cast<uint32_t>(std::strtoul(content.c_str() + start, nullptr, 10));
        end = content.find(':', start);
        if (end == content.npos)
            break;
        start = end + 1;

        result.emplace_back(username, uid, gid);

        // Advance to next line
        end = content.find('\n', start);
        if (end == content.npos)
            break;
        start = end + 1;
    }

    return result;
}

QString WslUtil::getUsername(const std::wstring &distName, uint32_t uid)
{
    auto passwd = readPasswdFile(distName);
    for (const auto &user : passwd) {
        if (std::get<1>(user) == uid)
            return QString::fromStdString(std::get<0>(user));
    }
    return QString::null;
}

uint32_t WslUtil::getUid(const std::wstring &distName, const QString &username)
{
    auto passwd = readPasswdFile(distName);
    const std::string u8Username = username.toStdString();
    for (const auto &user : passwd) {
        if (std::get<0>(user) == u8Username)
            return std::get<1>(user);
    }
    return INVALID_UID;
}
