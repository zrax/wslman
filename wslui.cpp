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

#include "wslui.h"

#include "wslregistry.h"
#include <QListWidget>
#include <QSplitter>
#include <QToolBar>
#include <QMessageBox>
#include <process.h>

enum { DistUuidRole = Qt::UserRole };

WslUi::WslUi()
    : m_registry()
{
    m_distList = new QListWidget(this);

    auto split = new QSplitter(this);
    split->addWidget(m_distList);

    setCentralWidget(split);

    m_openShell = new QAction(QIcon(":/icons/terminal.png"), tr("Open Shell"), this);
    m_openShell->setEnabled(false);

    auto toolbar = addToolBar(tr("Show Toolbar"));
    toolbar->setMovable(false);
    toolbar->toggleViewAction()->setEnabled(false);
    toolbar->setIconSize(QSize(32, 32));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->addAction(m_openShell);

    connect(m_distList, &QListWidget::currentItemChanged, this, &WslUi::distSelected);
    connect(m_distList, &QListWidget::itemActivated, this, &WslUi::distActivated);
    connect(m_openShell, &QAction::triggered, this, [this](bool) {
        distActivated(m_distList->currentItem());
    });

    try {
        m_registry = new WslRegistry;
        for (const auto &dist : m_registry->getDistributions()) {
            auto item = new QListWidgetItem(QString::fromStdWString(dist.name()), m_distList);
            item->setData(DistUuidRole, QString::fromStdWString(dist.uuidString()));
        }
        m_distList->sortItems();
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to get distribution list: %1").arg(err.what()));
    }

    try {
        auto defaultDist = m_registry->defaultDistribution();
        if (defaultDist.isValid()) {
            auto distUuid = QString::fromStdWString(defaultDist.name());
            m_distList->setCurrentItem(findDistByUuid(distUuid));
        } else if (m_distList->count() != 0) {
            m_distList->setCurrentItem(m_distList->item(0));
        }
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to query default distribution: %1").arg(err.what()));
    }
}

WslUi::~WslUi()
{
    delete m_registry;
}

void WslUi::distSelected(QListWidgetItem *current, QListWidgetItem *)
{
    if (!current) {
        m_openShell->setEnabled(false);
        return;
    }
    m_openShell->setEnabled(true);

    auto uuid = current->data(DistUuidRole).toString().toStdWString();
    try {
        WslDistribution dist = WslRegistry::findDistByUuid(uuid);
        if (dist.isValid()) {
            // TODO
        }
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to query distribution %1: %2")
                .arg(current->text()).arg(err.what()));
    }
}

static unsigned _startShell(void *context)
{
    std::wstring *distName = reinterpret_cast<std::wstring *>(context);

    DWORD exitCode;
    WslApi::LaunchInteractive(distName->c_str(), L"", FALSE, &exitCode);
    delete distName;
    return exitCode;
}

void WslUi::distActivated(QListWidgetItem *item)
{
    if (!item)
        return;

    auto uuid = item->data(DistUuidRole).toString().toStdWString();
    try {
        WslDistribution dist = WslRegistry::findDistByUuid(uuid);
        if (dist.isValid()) {
            auto heapName = new std::wstring(dist.name());
            unsigned threadId;
            HANDLE th = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0,
                                &_startShell, reinterpret_cast<void *>(heapName),
                                0, &threadId));
            CloseHandle(th);
        }
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to start distribution %1: %2")
                .arg(item->text()).arg(err.what()));
    }
}

QListWidgetItem *WslUi::findDistByUuid(const QString &uuid)
{
    for (int i = 0; i < m_distList->count(); ++i) {
        QListWidgetItem *item = m_distList->item(i);
        if (m_distList->item(i)->data(DistUuidRole).toString() == uuid)
            return item;
    }
    return nullptr;
}
