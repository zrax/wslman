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
#include "wslutils.h"
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QVBoxLayout>

WslSetUser::WslSetUser(const std::wstring &distName, QWidget *parent)
    : QDialog(parent), m_distName(distName)
{
    setWindowTitle(tr("Set Default UID"));

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
    QString user = WslUtil::getUsername(m_distName, uid);
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

    return WslUtil::getUid(m_distName, m_userEntry->text());
}
