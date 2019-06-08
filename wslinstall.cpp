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

#include "wslinstall.h"

#include "wslregistry.h"
#include "wslui.h"
#include "wslutils.h"
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QToolButton>
#include <QGridLayout>
#include <QCompleter>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

WslInstallDialog::WslInstallDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Install WSL Distribution"));

    auto lblName = new QLabel(tr("&Name:"), this);
    m_distName = new QLineEdit(this);
    lblName->setBuddy(m_distName);
    m_distIconLabel = new QLabel(this);
    m_distIcon = QIcon(":/icons/terminal.ico");
    m_distIconLabel->setPixmap(m_distIcon.pixmap(32, 32));

    connect(m_distName, &QLineEdit::textChanged, this, [this](const QString &name) {
        m_distIcon = WslUi::pickDistIcon(name);
        m_distIconLabel->setPixmap(m_distIcon.pixmap(32, 32));
    });

    auto lblPath = new QLabel(tr("Install &Path:"), this);
    m_installPath = new QLineEdit(this);
    auto dirModel = new QFileSystemModel(m_installPath);
    dirModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    dirModel->setRootPath(QDir::rootPath());
    auto installPathCompleter = new QCompleter(dirModel, m_installPath);
    m_installPath->setCompleter(installPathCompleter);
    lblPath->setBuddy(m_installPath);
    auto selectInstallPath = new QToolButton(this);
    const QIcon openIcon(":/icons/document-open.ico");
    selectInstallPath->setIconSize(QSize(16, 16));
    selectInstallPath->setIcon(openIcon);

    auto lblTarball = new QLabel(tr("Install &Tarball:"), this);
    m_tarball = new QLineEdit(this);
    auto fileModel = new QFileSystemModel(m_tarball);
    fileModel->setFilter(QDir::AllDirs | QDir::AllEntries | QDir::NoDotAndDotDot);
    fileModel->setNameFilters(QStringList{QStringLiteral("*.tar.gz"), QStringLiteral("*.tgz")});
    fileModel->setRootPath(QDir::rootPath());
    auto tarballCompleter = new QCompleter(fileModel, m_tarball);
    m_tarball->setCompleter(tarballCompleter);
    lblTarball->setBuddy(m_tarball);
    auto selectTarball = new QToolButton(this);
    selectTarball->setIconSize(QSize(16, 16));
    selectTarball->setIcon(openIcon);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("&Install"));

    connect(selectInstallPath, &QAbstractButton::clicked, this, [this](bool) {
        QString path = QFileDialog::getExistingDirectory(this,
                            tr("Select Install Path..."), m_installPath->text());
        if (!path.isEmpty())
            m_installPath->setText(path);
    });

    connect(selectTarball, &QAbstractButton::clicked, this, [this](bool) {
        QString path = QFileDialog::getOpenFileName(this,
                            tr("Select Install Tarball..."), m_tarball->text(),
                            QStringLiteral("Gzipped Tarballs (*.tar.gz *.tgz)"));
        if (!path.isEmpty())
            m_tarball->setText(path);
    });

    auto layout = new QGridLayout(this);
    layout->addWidget(lblName, 0, 0);
    layout->addWidget(m_distName, 0, 1);
    layout->addWidget(m_distIconLabel, 0, 2);
    layout->addWidget(lblPath, 1, 0);
    layout->addWidget(m_installPath, 1, 1);
    layout->addWidget(selectInstallPath, 1, 2);
    layout->addWidget(lblTarball, 2, 0);
    layout->addWidget(m_tarball, 2, 1);
    layout->addWidget(selectTarball, 2, 2);
    layout->addItem(new QSpacerItem(0, 10, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding),
                    3, 0, 1, 3);
    layout->addWidget(buttons, 4, 0, 1, 3);
}

static bool isDirectoryEmpty(const QString &path)
{
    const QFileInfoList entries = QDir(path).entryInfoList(
            QDir::NoDotAndDotDot | QDir::AllEntries | QDir::AllDirs
            | QDir::Hidden | QDir::System);
    return entries.count() == 0;
}

bool WslInstallDialog::validate()
{
    if (m_distName->text().isEmpty() || m_installPath->text().isEmpty()
            || m_tarball->text().isEmpty()) {
        QMessageBox::critical(this, QString::null, tr("All fields are required"));
        return false;
    }

    std::wstring distName = m_distName->text().toStdWString();
    WslDistribution dist;
    try {
        WslRegistry registry;
        dist = registry.findDistByName(distName);
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to query WSL distributions: %1").arg(err.what()));
        return false;
    }
    if (dist.isValid()) {
        QMessageBox::critical(this, QString::null,
                tr("A distribution named \"%1\" already exists").arg(distName));
        return false;
    }

    QString installPath = m_installPath->text();
    if (QFileInfo(installPath).exists() && !isDirectoryEmpty(installPath)) {
        QMessageBox::critical(this, QString::null,
                tr("The install path \"%1\" already exists and is not empty").arg(installPath));
        return false;
    }

    QString tarball = m_tarball->text();
    QFileInfo tarballInfo(tarball);
    if (!tarballInfo.exists() || !tarballInfo.isFile() || !tarballInfo.isReadable()) {
        QMessageBox::critical(this, QString::null,
                tr("The specified tarball \"%1\" cannot be read").arg(tarball));
        return false;
    }

    return true;
}

static QString wslError(const QString &prefix, HRESULT rc)
{
    wchar_t buffer[512];
    swprintf(buffer, std::size(buffer), L"0x%08x", rc);
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, rc,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buffer, static_cast<DWORD>(std::size(buffer)), nullptr);

    return QStringLiteral("%1: %2").arg(prefix).arg(QString::fromWCharArray(buffer));
}

struct Pushd
{
    Pushd(const QString &path)
    {
        lastDir = QDir::currentPath();
        result = QDir::setCurrent(path);
    }

    ~Pushd()
    {
        QDir::setCurrent(lastDir);
    }

    QString lastDir;
    bool result;

    operator bool() const { return result; }
};

void WslInstallDialog::performInstall()
{
    std::wstring distName = m_distName->text().toStdWString();
    AllocConsole();
    auto context = WslConsoleContext::createConsole(distName, m_distIcon);

    setupDistribution();

    context->unref();
    FreeConsole();
}

void WslInstallDialog::setupDistribution()
{
    std::wstring distName = m_distName->text().toStdWString();
    wprintf(L"Installing %s...\n", distName.c_str());
    try {
        QString installPath = m_installPath->text();
        if (!QDir::current().mkpath(installPath)) {
            QMessageBox::critical(this, QString::null,
                    tr("Failed to create distribution directory"));
            return;
        }

        Pushd cdDist(m_installPath->text());
        if (!cdDist) {
            QMessageBox::critical(this, QString::null,
                    tr("Failed to create distribution directory"));
            return;
        }

        std::wstring tarball = m_tarball->text().toStdWString();
        // TODO:  This DOESN'T WORK, because it can only be used when the
        // caller is an (installed) UWP application.  FFS Microsoft.
        auto rc = WslApi::RegisterDistribution(distName.c_str(), tarball.c_str());
        if (FAILED(rc)) {
            QMessageBox::critical(this, QString::null,
                    wslError(tr("Failed to register distribution"), rc));
            return;
        }

        WslRegistry registry;
        WslDistribution dist = registry.findDistByName(distName);
        if (!dist.isValid()) {
            QMessageBox::critical(this, QString::null, tr("Failed to register distribution"));
            return;
        }

        // Delete /etc/resolv.conf to allow WSL to generate a version based on
        // Windows networking information.
        DWORD exitCode;
        rc = WslApi::LaunchInteractive(distName.c_str(),
                        L"/bin/rm /etc/resolv.conf", TRUE, &exitCode);
        if (FAILED(rc)) {
            QMessageBox::critical(this, QString::null,
                    wslError(tr("Failed to configure distribution"), rc));
            return;
        }

        // Create a default user
        QString username;
        for ( ;; ) {
            bool ok;
            username = QInputDialog::getText(this, tr("Create Default User"),
                            tr("Enter a Linux username.  This does not need to match your Windows username."),
                            QLineEdit::Normal, QString::null, &ok);
            if (!ok || username.isEmpty()) {
                // User cancelled the dialog -- don't create a user
                username = QString::null;
                break;
            }

            username.replace(QLatin1Char('\''), QLatin1String("'\\''"));
            std::wstring commandLine = QStringLiteral("/usr/sbin/adduser --quiet --gecos '' '%1'")
                                            .arg(username).toStdWString();
            rc = WslApi::LaunchInteractive(distName.c_str(), commandLine.c_str(),
                            TRUE, &exitCode);
            if (FAILED(rc)) {
                QMessageBox::critical(this, QString::null,
                        wslError(tr("Failed to create default user"), rc));
                return;
            } else if (exitCode == 0) {
                // Successfully created the user
                break;
            }
        }
        if (username.isEmpty()) {
            rc = WslApi::ConfigureDistribution(distName.c_str(), 0, WslApi::DistributionFlags_All);
        } else {
            uint32_t uid = WslUtil::getUid(distName, username);
            rc = WslApi::ConfigureDistribution(distName.c_str(), uid, WslApi::DistributionFlags_All);
        }
        if (FAILED(rc)) {
            QMessageBox::critical(this, QString::null,
                    wslError(tr("Failed to configure default user"), rc));
            return;
        }
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to register distribution: %1").arg(err.what()));
        return;
    }
}
