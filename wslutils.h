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

namespace WslUtil
{
    QString getUsername(const std::wstring &distName, uint32_t uid);
    uint32_t getUid(const std::wstring &distName, const QString &username);
}
