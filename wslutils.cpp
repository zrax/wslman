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

#include "wslwrap.h"
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

QString WslUtil::getUsername(const std::wstring &distName, uint32_t uid)
{
    UniqueHandle hStdoutRead, hStdoutWrite;
    SECURITY_ATTRIBUTES secattr{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (!CreatePipe(hStdoutRead.receive(), hStdoutWrite.receive(), &secattr, 0))
        return QString::null;

    auto cmd = QStringLiteral("/usr/bin/getent passwd %1").arg(uid);
    UniqueHandle hProcess;
    try {
        auto rc = WslApi::Launch(distName.c_str(), cmd.toStdWString().c_str(),
                                 false, GetStdHandle(STD_INPUT_HANDLE), hStdoutWrite.get(),
                                 GetStdHandle(STD_ERROR_HANDLE), hProcess.receive());
        if (FAILED(rc))
            return QString::null;
    } catch (const std::runtime_error &) {
        return QString::null;
    }

    WaitForSingleObject(hProcess.get(), INFINITE);
    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess.get(), &exitCode))
        return QString::null;
    if (exitCode != 0) {
        // TODO: Try a different approach or path if applicable
        return QString::null;
    }

    char buffer[512];
    DWORD nRead;
    if (!ReadFile(hStdoutRead.get(), buffer, sizeof(buffer) - 1, &nRead, nullptr))
        return QString::null;

    buffer[nRead] = 0;
    auto result = QString::fromUtf8(buffer);
    int end = result.indexOf(QLatin1Char(':'));
    if (end < 0)
        return QString::null;
    return result.left(end);
}

uint32_t WslUtil::getUid(const std::wstring &distName, const QString &username)
{
    UniqueHandle hStdoutRead, hStdoutWrite;
    SECURITY_ATTRIBUTES secattr{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (!CreatePipe(hStdoutRead.receive(), hStdoutWrite.receive(), &secattr, 0))
        return INVALID_UID;

    QString cleanUser = username;
    cleanUser.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    auto cmd = QStringLiteral("/usr/bin/id -u '%1'").arg(cleanUser);
    UniqueHandle hProcess;
    try {
        auto rc = WslApi::Launch(distName.c_str(), cmd.toStdWString().c_str(),
                                 false, GetStdHandle(STD_INPUT_HANDLE), hStdoutWrite.get(),
                                 GetStdHandle(STD_ERROR_HANDLE), hProcess.receive());
        if (FAILED(rc))
            return INVALID_UID;
    } catch (const std::runtime_error &) {
        return INVALID_UID;
    }

    WaitForSingleObject(hProcess.get(), INFINITE);
    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess.get(), &exitCode) || exitCode != 0)
        return INVALID_UID;

    char buffer[512];
    DWORD nRead;
    if (!ReadFile(hStdoutRead.get(), buffer, sizeof(buffer) - 1, &nRead, nullptr))
        return INVALID_UID;

    buffer[nRead] = 0;
    auto result = QString::fromUtf8(buffer);
    bool ok;
    uint uid = result.toUInt(&ok, 10);
    return ok ? uid : INVALID_UID;
}
