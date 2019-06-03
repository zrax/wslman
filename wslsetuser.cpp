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

#include "wslsetuser.h"

#include "wslregistry.h"
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QVBoxLayout>

// (Ab)use unique_ptr to ensure handles get closed at function return
static std::unique_ptr<void, decltype(&CloseHandle)>
makeHandleCloser(HANDLE handle)
{
    return {handle, &CloseHandle};
}

WslSetUser::WslSetUser(const std::wstring &distName, QWidget *parent)
    : QDialog(parent), m_distName(distName)
{
    auto lblUserEntry = new QLabel(tr("&Username or UID:"), this);
    m_userEntry = new QLineEdit(this);
    lblUserEntry->setBuddy(m_userEntry);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(lblUserEntry);
    layout->addWidget(m_userEntry);
    layout->addItem(new QSpacerItem(0, 5, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    layout->addWidget(buttons);
}

void WslSetUser::setUID(uint32_t uid)
{
    QString user = getUsername(uid);
    if (user.isEmpty())
        user = QString::number(uid);
    m_userEntry->setText(user);
}

uint32_t WslSetUser::getUID() const
{
    if (m_userEntry->text().isEmpty())
        return INVALID_UID;

    bool ok;
    uint numeric = m_userEntry->text().toUInt(&ok, 10);
    if (ok)
        return numeric;

    HANDLE hStdoutRead, hStdoutWrite;
    SECURITY_ATTRIBUTES secattr{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &secattr, 0))
        return INVALID_UID;

    auto hcStdoutRead = makeHandleCloser(hStdoutRead);
    auto hcStdoutWrite = makeHandleCloser(hStdoutWrite);

    auto cleanEntry = m_userEntry->text().replace(QLatin1Char('\''), QLatin1String("'\\''"));
    auto cmd = QStringLiteral("/usr/bin/id -u '%1'").arg(cleanEntry);
    HANDLE hProcess;
    try {
        auto rc = WslApi::Launch(m_distName.c_str(), cmd.toStdWString().c_str(),
                                 false, GetStdHandle(STD_INPUT_HANDLE), hStdoutWrite,
                                 GetStdHandle(STD_ERROR_HANDLE), &hProcess);
        if (FAILED(rc))
            return INVALID_UID;
    } catch (const std::runtime_error &) {
        return INVALID_UID;
    }

    auto hcProcess = makeHandleCloser(hProcess);

    WaitForSingleObject(hProcess, INFINITE);
    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode) || exitCode != 0)
        return INVALID_UID;

    char buffer[512];
    DWORD nRead;
    if (!ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &nRead, nullptr))
        return INVALID_UID;

    buffer[nRead] = 0;
    auto result = QString::fromUtf8(buffer);
    uint uid = result.toUInt(&ok, 10);
    return ok ? uid : INVALID_UID;
}

QString WslSetUser::getUsername(uint32_t uid)
{
    HANDLE hStdoutRead, hStdoutWrite;
    SECURITY_ATTRIBUTES secattr{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &secattr, 0))
        return QString::null;

    auto hcStdoutRead = makeHandleCloser(hStdoutRead);
    auto hcStdoutWrite = makeHandleCloser(hStdoutWrite);

    auto cmd = QStringLiteral("/usr/bin/getent passwd %1").arg(uid);
    HANDLE hProcess;
    try {
        auto rc = WslApi::Launch(m_distName.c_str(), cmd.toStdWString().c_str(),
                                 false, GetStdHandle(STD_INPUT_HANDLE), hStdoutWrite,
                                 GetStdHandle(STD_ERROR_HANDLE), &hProcess);
        if (FAILED(rc))
            return QString::null;
    } catch (const std::runtime_error &) {
        return QString::null;
    }

    auto hcProcess = makeHandleCloser(hProcess);

    WaitForSingleObject(hProcess, INFINITE);
    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode))
        return QString::null;
    if (exitCode != 0) {
        // TODO: Try a different approach or path if applicable
        return QString::null;
    }

    char buffer[512];
    DWORD nRead;
    if (!ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &nRead, nullptr))
        return QString::null;

    buffer[nRead] = 0;
    auto result = QString::fromUtf8(buffer);
    int end = result.indexOf(QLatin1Char(':'));
    if (end < 0)
        return QString::null;
    return result.left(end);
}
