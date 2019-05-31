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
#include <QToolButton>
#include <QTreeWidget>
#include <QFrame>
#include <QSplitter>
#include <QGridLayout>
#include <QMessageBox>
#include <process.h>
#include <atomic>

enum {
    DistUuidRole = Qt::UserRole,
    DistNameRole,
    EnvSavedKeyRole,
};

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

    m_distDetails = new QFrame(this);
    m_distDetails->setFrameStyle(QFrame::StyledPanel);
    m_distDetails->setBackgroundRole(QPalette::Base);
    m_distDetails->setAutoFillBackground(true);
    m_distDetails->setEnabled(false);
    auto distLayout = new QGridLayout(m_distDetails);
    distLayout->setContentsMargins(5, 5, 5, 5);

    auto lblName = new QLabel(tr("Name:"), this);
    m_name = new QLabel(this);
    distLayout->addWidget(lblName, 0, 0);
    distLayout->addWidget(m_name, 0, 1);

    auto lblVersion = new QLabel(tr("Version:"), this);
    m_version = new QLabel(this);
    distLayout->addWidget(lblVersion, 1, 0);
    distLayout->addWidget(m_version, 1, 1);

    distLayout->addItem(new QSpacerItem(0, 10), 2, 0, 1, 2);
    auto lblDefaultUser = new QLabel(tr("Default &User:"), this);
    auto userContainer = new QWidget(this);
    auto userLayout = new QHBoxLayout(userContainer);
    userLayout->setContentsMargins(0, 0, 0, 0);
    distLayout->addWidget(lblDefaultUser, 3, 0);
    distLayout->addWidget(userContainer, 3, 1, 1, 2);

    m_defaultUser = new QLabel(userContainer);
    auto editDefaultUser = new QToolButton(userContainer);
    editDefaultUser->setText(QStringLiteral("..."));
    lblDefaultUser->setBuddy(editDefaultUser);
    userLayout->addWidget(m_defaultUser);
    userLayout->addWidget(editDefaultUser);
    userLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Minimum));

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
    auto editKernelCmdLine = new QToolButton(this);
    editKernelCmdLine->setText(QStringLiteral("..."));
    distLayout->addWidget(lblKernelCmdLine, 9, 0);
    distLayout->addWidget(m_kernelCmdLine, 9, 1);
    distLayout->addWidget(editKernelCmdLine, 9, 2);

    distLayout->addItem(new QSpacerItem(0, 20), 10, 0, 1, 2);
    auto lblDefaultEnv = new QLabel(tr("Default &Environment:"), this);
    m_defaultEnvironment = new QTreeWidget(this);
    m_defaultEnvironment->setHeaderLabels(QStringList{tr("Environment"), tr("Value")});
    m_defaultEnvironment->setRootIsDecorated(false);
    lblDefaultEnv->setBuddy(m_defaultEnvironment);
    distLayout->addWidget(lblDefaultEnv, 11, 0, 1, 2);
    distLayout->addWidget(m_defaultEnvironment, 12, 0, 1, 2);

    auto envButtons = new QWidget(this);
    auto envLayout = new QVBoxLayout(envButtons);
    envLayout->setContentsMargins(0, 0, 0, 0);
    distLayout->addWidget(envButtons, 12, 2);

    m_envAdd = new QToolButton(envButtons);
    m_envAdd->setIconSize(QSize(16, 16));
    m_envAdd->setIcon(QIcon(":/icons/list-add.png"));
    m_envEdit = new QToolButton(envButtons);
    m_envEdit->setIconSize(QSize(16, 16));
    m_envEdit->setIcon(QIcon(":/icons/document-edit.png"));
    m_envEdit->setEnabled(false);
    m_envDel = new QToolButton(envButtons);
    m_envDel->setIconSize(QSize(16, 16));
    m_envDel->setIcon(QIcon(":/icons/edit-delete.png"));
    m_envDel->setEnabled(false);

    envLayout->addWidget(m_envAdd);
    envLayout->addWidget(m_envEdit);
    envLayout->addWidget(m_envDel);
    envLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));

    auto split = new QSplitter(this);
    split->addWidget(m_distList);
    split->addWidget(m_distDetails);
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

    connect(m_enableInterop, &QCheckBox::clicked, this, &WslUi::commitDistFlags);
    connect(m_appendNTPath, &QCheckBox::clicked, this, &WslUi::commitDistFlags);
    connect(m_enableDriveMounting, &QCheckBox::clicked, this, &WslUi::commitDistFlags);
    connect(m_kernelCmdLine, &QLineEdit::editingFinished, this, &WslUi::commitKernelCmdLine);

    connect(m_defaultEnvironment, &QTreeWidget::currentItemChanged,
            this, &WslUi::environSelected);
    connect(m_defaultEnvironment, &QTreeWidget::itemChanged,
            this, &WslUi::environChanged);

    connect(m_envAdd, &QToolButton::clicked, this, [this](bool) {
        m_defaultEnvironment->blockSignals(true);
        auto envItem = new QTreeWidgetItem(m_defaultEnvironment);
        envItem->setFlags(envItem->flags() | Qt::ItemIsEditable);
        m_defaultEnvironment->blockSignals(false);
        m_defaultEnvironment->editItem(envItem, 0);
    });
    connect(m_envEdit, &QToolButton::clicked, this, [this](bool) {
        QTreeWidgetItem *item = m_defaultEnvironment->currentItem();
        if (item)
            m_defaultEnvironment->editItem(item, 1);
    });
    connect(m_envDel, &QToolButton::clicked, this, &WslUi::deleteSelectedEnviron);

    try {
        m_registry = new WslRegistry;
        for (const auto &dist : m_registry->getDistributions()) {
            auto distName = QString::fromStdWString(dist.name());
            auto item = new QListWidgetItem(distName, m_distList);
            item->setData(DistUuidRole, QString::fromStdWString(dist.uuid()));
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
            auto distUuid = QString::fromStdWString(defaultDist.uuid());
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

    m_openShell->setEnabled(false);
    m_distDetails->setEnabled(false);

    WslDistribution dist = getDistribution(current);
    if (dist.isValid()) {
        updateDistProperties(dist);
        m_openShell->setEnabled(true);
        m_distDetails->setEnabled(true);
    }
}

struct _startShell_Context
{
    std::wstring distName;
    std::atomic<bool> shellTerminated = false;
    QString errorMessage;

    // One ref for the UI thread and one for the _startShell thread
    std::atomic<int> refs = 2;
    void unref() {
        if (--refs == 0)
            delete this;
    }
};

static unsigned _startShell(void *pvContext)
{
    auto context = reinterpret_cast<_startShell_Context *>(pvContext);

    DWORD exitCode;
    try {
        WslApi::LaunchInteractive(context->distName.c_str(), L"", FALSE, &exitCode);
    } catch (const std::runtime_error &err) {
        context->errorMessage = QObject::tr("Failed to start %1: %2")
                                .arg(context->distName).arg(err.what());
    }

    context->shellTerminated = true;
    context->unref();
    return exitCode;
}

void WslUi::distActivated(QListWidgetItem *item)
{
    WslDistribution dist = getDistribution(item);
    if (dist.isValid()) {
        AllocConsole();
        SetConsoleTitleW(dist.name().c_str());

        auto context = new _startShell_Context;
        context->distName = dist.name();
        unsigned threadId;
        HANDLE th = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0,
                            &_startShell, reinterpret_cast<void *>(context),
                            0, &threadId));
        CloseHandle(th);

        // Detach from the console once the WSL process starts using it
        DWORD processList[4];
        DWORD processCount = 4;
        for ( ;; ) {
            DWORD activeProcs = GetConsoleProcessList(processList, processCount);
            if (activeProcs > 1 || context->shellTerminated)
                break;
            Sleep(100);
        }
        if (!context->errorMessage.isEmpty())
            QMessageBox::critical(this, QString::null, context->errorMessage);
        context->unref();
        FreeConsole();
    }
}

void WslUi::commitDistFlags(bool)
{
    WslDistribution dist = getDistribution(m_distList->currentItem());
    if (dist.isValid()) {
        // Preserve any flags we don't know about...
        int flags = dist.flags() & ~WslApi::DistributionFlags_KnownMask;
        if (m_enableInterop->isChecked())
            flags |= WslApi::DistributionFlags_EnableInterop;
        if (m_appendNTPath->isChecked())
            flags |= WslApi::DistributionFlags_AppendNTPath;
        if (m_enableDriveMounting->isChecked())
            flags |= WslApi::DistributionFlags_EnableDriveMounting;

        try {
            dist.setFlags(static_cast<WslApi::DistributionFlags>(flags));
        } catch (const std::runtime_error &err) {
            QMessageBox::critical(this, QString::null,
                    tr("Failed to set property: %1").arg(err.what()));
        }
        updateDistProperties(dist);
    }
}

void WslUi::commitKernelCmdLine()
{
    WslDistribution dist = getDistribution(m_distList->currentItem());
    if (dist.isValid()) {
        std::wstring cmdline = m_kernelCmdLine->text().toStdWString();
        try {
            dist.setKernelCmdLine(cmdline);
        } catch (const std::runtime_error &err) {
            QMessageBox::critical(this, QString::null,
                    tr("Failed to set property: %1").arg(err.what()));
        }
        updateDistProperties(dist);
    }
}

void WslUi::environSelected(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    if (current) {
        m_envEdit->setEnabled(true);
        m_envDel->setEnabled(true);
    } else {
        m_envEdit->setEnabled(false);
        m_envDel->setEnabled(false);
    }
}

void WslUi::environChanged(QTreeWidgetItem *item, int column)
{
    WslDistribution dist = getDistribution(m_distList->currentItem());
    if (dist.isValid()) {
        try {
            if (column == 0) {
                // Changed the key...  Delete the old one if it exists
                const QString oldKey = item->data(0, EnvSavedKeyRole).toString();
                if (!oldKey.isEmpty())
                    dist.delEnvironment(oldKey.toStdWString());

                m_defaultEnvironment->blockSignals(true);
                item->setData(0, EnvSavedKeyRole, item->text(0));
                m_defaultEnvironment->blockSignals(false);
            }
            const QString key = item->text(0);
            const QString value = item->text(1);
            if (!key.isEmpty())
                dist.addEnvironment(key.toStdWString(), value.toStdWString());
        } catch (const std::runtime_error &err) {
            QMessageBox::critical(this, QString::null,
                    tr("Failed to set environment variable: %1").arg(err.what()));
        }
        updateDistProperties(dist);
    }
}

void WslUi::deleteSelectedEnviron(bool)
{
    QTreeWidgetItem *envItem = m_defaultEnvironment->currentItem();
    WslDistribution dist = getDistribution(m_distList->currentItem());
    if (envItem && dist.isValid()) {
        QString envKey = envItem->data(0, EnvSavedKeyRole).toString();
        try {
            dist.delEnvironment(envKey.toStdWString());
        } catch (const std::runtime_error &err) {
            QMessageBox::critical(this, QString::null,
                    tr("Failed to delete environment variable: %1").arg(err.what()));
        }
        updateDistProperties(dist);
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

void WslUi::updateDistProperties(const WslDistribution &dist)
{
    m_name->setText(QString::fromStdWString(dist.name()));
    m_version->setText(QString::number(dist.version()));
    m_defaultUser->setText(tr("Unknown (%1)").arg(dist.defaultUID()));
    m_location->setText(QString::fromStdWString(dist.path()));
    m_enableInterop->setChecked(dist.flags() & WslApi::DistributionFlags_EnableInterop);
    m_appendNTPath->setChecked(dist.flags() & WslApi::DistributionFlags_AppendNTPath);
    m_enableDriveMounting->setChecked(dist.flags() & WslApi::DistributionFlags_EnableDriveMounting);
    m_kernelCmdLine->setText(QString::fromStdWString(dist.kernelCmdLine()));

    m_defaultEnvironment->clear();
    for (const std::wstring &envLine : dist.defaultEnvironment()) {
        auto env = QString::fromStdWString(envLine);
        QStringList parts = env.split(QLatin1Char('='));
        if (parts.count() != 2) {
            QMessageBox::critical(this, QString::null,
                            tr("Invalid environment line: %1").arg(env));
            continue;
        }

        m_defaultEnvironment->blockSignals(true);
        auto envItem = new QTreeWidgetItem(m_defaultEnvironment, parts);
        envItem->setFlags(envItem->flags() | Qt::ItemIsEditable);
        envItem->setData(0, EnvSavedKeyRole, parts.first());
        m_defaultEnvironment->blockSignals(false);
    }
}

WslDistribution WslUi::getDistribution(QListWidgetItem *item)
{
    if (item) {
        auto uuid = item->data(DistUuidRole).toString();
        auto distName = item->data(DistNameRole).toString();
        try {
            return WslRegistry::findDistByUuid(uuid.toStdWString());
        } catch (const std::runtime_error &err) {
            QMessageBox::critical(this, QString::null,
                    tr("Failed to query distribution %1: %2")
                    .arg(distName).arg(err.what()));
        }
    }

    return WslDistribution();
}
