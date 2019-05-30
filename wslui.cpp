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
#include <QToolBar>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QTreeWidget>
#include <QFrame>
#include <QSplitter>
#include <QGridLayout>
#include <QMessageBox>
#include <process.h>

enum { DistUuidRole = Qt::UserRole, DistNameRole };

static QIcon pickDistIcon(const QString &name)
{
    if (name.contains(QLatin1String("Alpine"), Qt::CaseInsensitive))
        return QIcon(":/icons/dist-alpine.png");
    else if (name.contains(QLatin1String("Arch"), Qt::CaseInsensitive))
        return QIcon(":/icons/dist-arch.png");
    else if (name.contains(QLatin1String("Debian"), Qt::CaseInsensitive))
        return QIcon(":/icons/dist-debian.png");
    else if (name.contains(QLatin1String("Fedora"), Qt::CaseInsensitive))
        return QIcon(":/icons/dist-fedora.png");
    else if (name.contains(QLatin1String("Gentoo"), Qt::CaseInsensitive))
        return QIcon(":/icons/dist-gentoo.png");
    else if (name.contains(QRegularExpression("Red.?Hat", QRegularExpression::CaseInsensitiveOption))
             || name.contains(QLatin1String("RHEL"), Qt::CaseInsensitive))
        return QIcon(":/icons/dist-redhat.png");
    else if (name.contains(QLatin1String("SUSE"), Qt::CaseInsensitive)
             || name.contains(QLatin1String("SLES"), Qt::CaseInsensitive))
        return QIcon(":/icons/dist-suse.png");
    else if (name.contains(QLatin1String("Ubuntu"), Qt::CaseInsensitive))
        return QIcon(":/icons/dist-ubuntu.png");
    return QIcon(":/icons/terminal.ico");
}

WslUi::WslUi()
    : m_registry()
{
    setWindowTitle(tr("WSL Distribution Manager"));

    m_distList = new QListWidget(this);
    m_distList->setIconSize(QSize(32, 32));

    auto distDetails = new QFrame(this);
    distDetails->setFrameStyle(QFrame::StyledPanel);
    distDetails->setBackgroundRole(QPalette::Base);
    distDetails->setAutoFillBackground(true);
    auto distLayout = new QGridLayout(distDetails);
    distLayout->setContentsMargins(5, 5, 5, 5);

    auto lblName = new QLabel(tr("Name:"), this);
    m_name = new QLabel(this);
    distLayout->addWidget(lblName, 0, 0);
    distLayout->addWidget(m_name, 0, 1);

    auto lblVersion = new QLabel(tr("Version:"), this);
    m_version = new QLabel(this);
    distLayout->addWidget(lblVersion, 1, 0);
    distLayout->addWidget(m_version, 1, 1);

    auto lblDefaultUser = new QLabel(tr("Default User:"), this);
    m_defaultUser = new QLabel(this);
    distLayout->addWidget(lblDefaultUser, 2, 0);
    distLayout->addWidget(m_defaultUser, 2, 1);

    distLayout->addItem(new QSpacerItem(0, 10), 3, 0, 1, 2);
    auto lblLocation = new QLabel(tr("&Location:"), this);
    m_location = new QLineEdit(this);
    m_location->setReadOnly(true);
    lblLocation->setBuddy(m_location);
    distLayout->addWidget(lblLocation, 4, 0);
    distLayout->addWidget(m_location, 4, 1);

    auto lblFeatures = new QLabel(tr("&Features:"), this);
    m_enableInterop = new QCheckBox(tr("Allow executing Windows applications"), this);
    m_appendNTPath = new QCheckBox(tr("Add Windows PATH to environment"));
    m_enableDriveMounting = new QCheckBox(tr("Auto-mount Windows drives"));
    lblFeatures->setBuddy(m_enableInterop);
    distLayout->addWidget(lblFeatures, 5, 0);
    distLayout->addWidget(m_enableInterop, 5, 1);
    distLayout->addWidget(m_appendNTPath, 6, 1);
    distLayout->addWidget(m_enableDriveMounting, 7, 1);

    distLayout->addItem(new QSpacerItem(0, 10), 8, 0, 1, 2);
    auto lblKernelCmdLine = new QLabel(tr("&Kernel Command Line:"), this);
    m_kernelCmdLine = new QLineEdit(this);
    lblKernelCmdLine->setBuddy(m_kernelCmdLine);
    distLayout->addWidget(lblKernelCmdLine, 9, 0);
    distLayout->addWidget(m_kernelCmdLine, 9, 1);

    distLayout->addItem(new QSpacerItem(0, 20), 10, 0, 1, 2);
    auto lblDefaultEnv = new QLabel(tr("Default &Environment:"), this);
    m_defaultEnvironment = new QTreeWidget(this);
    m_defaultEnvironment->setHeaderLabels(QStringList{tr("Environment"), tr("Value")});
    m_defaultEnvironment->setRootIsDecorated(false);
    lblDefaultEnv->setBuddy(m_defaultEnvironment);
    distLayout->addWidget(lblDefaultEnv, 11, 0, 1, 2);
    distLayout->addWidget(m_defaultEnvironment, 12, 0, 1, 2);

    auto split = new QSplitter(this);
    split->addWidget(m_distList);
    split->addWidget(distDetails);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);

    setCentralWidget(split);

    m_openShell = new QAction(QIcon(":/icons/terminal.ico"), tr("Open Shell"), this);
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
            auto distName = QString::fromStdWString(dist.name());
            auto item = new QListWidgetItem(distName, m_distList);
            item->setData(DistUuidRole, QString::fromStdWString(dist.uuidString()));
            item->setData(DistNameRole, distName);
            item->setIcon(pickDistIcon(distName));
        }
        m_distList->sortItems();
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to get distribution list: %1").arg(err.what()));
    }

    try {
        auto defaultDist = m_registry->defaultDistribution();
        if (defaultDist.isValid()) {
            auto distUuid = QString::fromStdWString(defaultDist.uuidString());
            QListWidgetItem *defaultItem = findDistByUuid(distUuid);
            if (defaultItem)
                defaultItem->setText(tr("%1 (Default)").arg(defaultItem->text()));
            m_distList->setCurrentItem(defaultItem);
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
    m_name->setText(QString::null);
    m_version->setText(QString::null);
    m_defaultUser->setText(QString::null);
    m_location->setText(QString::null);
    m_enableInterop->setChecked(false);
    m_appendNTPath->setChecked(false);
    m_enableDriveMounting->setChecked(false);
    m_kernelCmdLine->setText(QString::null);
    m_defaultEnvironment->clear();

    if (!current) {
        m_openShell->setEnabled(false);
        return;
    }
    m_openShell->setEnabled(true);

    auto uuid = current->data(DistUuidRole).toString();
    auto distName = current->data(DistNameRole).toString();
    try {
        WslDistribution dist = WslRegistry::findDistByUuid(uuid.toStdWString());
        if (dist.isValid()) {
            m_name->setText(QString::fromStdWString(dist.name()));
            m_version->setText(QString::number(dist.version()));
            m_defaultUser->setText(tr("Unknown (%1)").arg(dist.defaultUID()));
            m_location->setText(QString::fromStdWString(dist.path()));
            m_enableInterop->setChecked(dist.flags() & WslApi::DistributionFlags_EnableInterop);
            m_appendNTPath->setChecked(dist.flags() & WslApi::DistributionFlags_AppendNTPath);
            m_enableDriveMounting->setChecked(dist.flags() & WslApi::DistributionFlags_EnableDriveMounting);
            m_kernelCmdLine->setText(QString::fromStdWString(dist.kernelCmdLine()));
            for (const std::wstring &envLine : dist.defaultEnvironment()) {
                auto env = QString::fromStdWString(envLine);
                QStringList parts = env.split(QLatin1Char('='));
                if (parts.count() != 2) {
                    QMessageBox::critical(this, QString::null,
                                    tr("Invalid environment line: %1").arg(env));
                    continue;
                }
                new QTreeWidgetItem(m_defaultEnvironment, parts);
            }
        }
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to query distribution %1: %2")
                .arg(distName).arg(err.what()));
    }
}

struct _startShell_Context
{
    std::wstring distName;
};

static unsigned _startShell(void *pvContext)
{
    auto context = reinterpret_cast<_startShell_Context *>(pvContext);

    DWORD exitCode;
    WslApi::LaunchInteractive(context->distName.c_str(), L"", FALSE, &exitCode);
    delete context;
    return exitCode;
}

void WslUi::distActivated(QListWidgetItem *item)
{
    if (!item)
        return;

    auto uuid = item->data(DistUuidRole).toString();
    auto distName = item->data(DistNameRole).toString();
    try {
        WslDistribution dist = WslRegistry::findDistByUuid(uuid.toStdWString());
        if (dist.isValid()) {
            auto context = new _startShell_Context;
            context->distName = dist.name();
            unsigned threadId;
            HANDLE th = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0,
                                &_startShell, reinterpret_cast<void *>(context),
                                0, &threadId));
            CloseHandle(th);
        }
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to start distribution %1: %2")
                .arg(distName).arg(err.what()));
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
