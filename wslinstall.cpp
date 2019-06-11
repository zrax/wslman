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
#include "wslfs.h"
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
#include <QProgressDialog>
#include <QMessageBox>
#include <archive.h>
#include <archive_entry.h>

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
    fileModel->setNameFilters({QStringLiteral("*.tar"), QStringLiteral("*.tar.*"),
                               QStringLiteral("*.tgz"), QStringLiteral("*.txz"),
                               QStringLiteral("*.tbz"), QStringLiteral("*.tbz2")});
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
                            QStringLiteral("Tarballs (*.tar *.tar.* *.tgz *.tbz *.tbz2 *.txz)"));
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

void WslInstallDialog::performInstall()
{
    std::wstring distName = m_distName->text().toStdWString();
    AllocConsole();
    auto context = WslConsoleContext::createConsole(distName, m_distIcon);

    setupDistribution();

    // Attempt to run the newly installed distribution
    context->startConsoleThread(this);
    FreeConsole();
}

#define BLOCK_SIZE 16384

static std::string archiveError(archive *arc)
{
    return std::string("Failed to extract archive: ") + archive_error_string(arc);
}

static void extractTarball(WslFs &rootfs, const std::wstring &tarball)
{
    std::unique_ptr<archive, decltype(&archive_read_free)> rootfsArchive(
        archive_read_new(), &archive_read_free
    );

    LARGE_INTEGER tarballSize;
    {
        UniqueHandle hTarball = CreateFileW(tarball.c_str(), MAXIMUM_ALLOWED,
                                    FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    0, nullptr);
        if (!GetFileSizeEx(hTarball.get(), &tarballSize))
            tarballSize.QuadPart = 0;
    }

    archive_read_support_filter_all(rootfsArchive.get());
    archive_read_support_format_all(rootfsArchive.get());
    if (archive_read_open_filename_w(rootfsArchive.get(), tarball.c_str(),
                                     BLOCK_SIZE) != ARCHIVE_OK)
        throw std::runtime_error(archiveError(rootfsArchive.get()));

    QProgressDialog progressDialog;
    progressDialog.setLabelText(QObject::tr("Installing distribution rootfs..."));
    // QProgressDialog only supports int progress, so adjust to KiB for a larger
    // possible maximum tarball length.
    progressDialog.setMaximum(static_cast<int>(tarballSize.QuadPart / 1024));
    progressDialog.setWindowModality(Qt::WindowModal);

    for ( ;; ) {
        if (progressDialog.wasCanceled())
            break;

        archive_entry *ent;
        int rc = archive_read_next_header(rootfsArchive.get(), &ent);
        if (rc == ARCHIVE_EOF)
            break;
        else if (rc != ARCHIVE_OK)
            throw std::runtime_error(archiveError(rootfsArchive.get()));

        auto progressKiB = archive_filter_bytes(rootfsArchive.get(), -1);
        progressDialog.setValue(static_cast<int>(progressKiB / 1024));

        std::string path;
        const char *u8path = archive_entry_pathname(ent);
        if (u8path) {
            path = u8path;
        } else {
            const wchar_t *wpath = archive_entry_pathname_w(ent);
            if (wpath)
                path = WslUtil::toUtf8(wpath);
            else
                throw std::runtime_error("Could not get pathname from archive entry");
        }
        if (starts_with(path, "./"))
            path = path.substr(1);
        else if (!starts_with(path, "/"))
            path = "/" + path;

        if (path == "/" || path == ".") {
            // We already created the root directory
            continue;
        }

        std::string hardLink;
        u8path = archive_entry_hardlink(ent);
        if (u8path) {
            hardLink = u8path;
        } else {
            const wchar_t *wpath = archive_entry_hardlink_w(ent);
            if (wpath)
                hardLink = WslUtil::toUtf8(wpath);
        }
        if (!hardLink.empty()) {
            if (starts_with(hardLink, "./"))
                hardLink = hardLink.substr(1);
            else if (!starts_with(hardLink, "/"))
                hardLink = "/" + hardLink;
            if (!rootfs.createHardLink(path, hardLink))
                throw std::runtime_error("Could not create hard link");
            continue;
        }

        auto astat = archive_entry_stat(ent);
        auto mtime_nsec = archive_entry_mtime_nsec(ent);
        WslAttr attr(astat->st_mode, astat->st_uid, astat->st_gid,
                     astat->st_mtime, mtime_nsec, astat->st_mtime, mtime_nsec,
                     astat->st_mtime, mtime_nsec);
        if (archive_entry_atime_is_set(ent)) {
            attr.atime = astat->st_atime;
            attr.atime_nsec = archive_entry_atime_nsec(ent);
        }
        if (archive_entry_ctime_is_set(ent)) {
            attr.ctime = astat->st_ctime;
            attr.ctime_nsec = archive_entry_ctime_nsec(ent);
        }

        auto type = archive_entry_filetype(ent);
        if (type == AE_IFDIR) {
            rootfs.createDirectory(path, attr);
        } else if (type == AE_IFLNK) {
            std::string symLink;
            u8path = archive_entry_symlink(ent);
            if (u8path) {
                symLink = u8path;
            } else {
                const wchar_t *wpath = archive_entry_symlink_w(ent);
                if (wpath)
                    symLink = WslUtil::toUtf8(wpath);
                else
                    throw std::runtime_error("Could not read archive symlink");
            }
            rootfs.createSymlink(path, symLink, attr);
        } else {
            UniqueHandle hFile = rootfs.createFile(path, attr);
            for ( ;; ) {
                const void *buffer;
                size_t size;
                int64_t offset;
                rc = archive_read_data_block(rootfsArchive.get(), &buffer, &size, &offset);
                if (rc == ARCHIVE_EOF)
                    break;
                else if (rc != ARCHIVE_OK)
                    throw std::runtime_error(archiveError(rootfsArchive.get()));

                DWORD nWritten;
                if (!WriteFile(hFile.get(), buffer, static_cast<DWORD>(size),
                               &nWritten, nullptr))
                    throw std::runtime_error("Could not write file data");
            }
        }
    }
}

void WslInstallDialog::setupDistribution()
{
    std::wstring distName = m_distName->text().toStdWString();
    wprintf(L"Installing %s...\n", distName.c_str());
    try {
        if (!QDir::current().mkpath(m_installPath->text())) {
            QMessageBox::critical(this, QString::null,
                    tr("Failed to create distribution directory"));
            return;
        }

        WslRegistry registry;
        std::wstring distDir = m_installPath->text().toStdWString();
        WslDistribution dist = registry.registerDistribution(distName.c_str(), distDir.c_str());
        if (!dist.isValid()) {
            QMessageBox::critical(this, QString::null, tr("Failed to register distribution"));
            return;
        }

        std::wstring tarball = m_tarball->text().toStdWString();
        auto rootfs = WslFs::create(dist.rootfsPath());
        dist.setVersion(rootfs.version());
        extractTarball(rootfs, tarball);

        // Delete /etc/resolv.conf to allow WSL to generate a version based on
        // Windows networking information.
        DWORD exitCode;
        auto rc = WslApi::LaunchInteractive(distName.c_str(),
                            L"/bin/rm /etc/resolv.conf", TRUE, &exitCode);
        if (FAILED(rc)) {
            QMessageBox::critical(this, QString::null,
                    wslError(tr("Failed to configure distribution"), rc));
            return;
        }

        // Create a default user
        QString username;
        std::wstring commandLine;
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
            commandLine = QStringLiteral("/usr/sbin/adduser -g '' '%1'")
                                .arg(username).toStdWString();
            rc = WslApi::LaunchInteractive(distName.c_str(), commandLine.c_str(),
                                           TRUE, &exitCode);
            if (FAILED(rc)) {
                QMessageBox::critical(this, QString::null,
                        wslError(tr("Failed to create default user"), rc));
                return;
            } else if (exitCode != 0) {
                QMessageBox::critical(this, QString::null,
                        tr("Failed to create default user with adduser"));
                return;
            } else {
                // Successfully created the user
                break;
            }
        }
        if (!username.isEmpty()) {
            const QStringList tryGroups{"adm", "wheel", "sudo", "cdrom", "plugdev"};
            for (const QString &group : tryGroups) {
                // These are allowed to fail since not all groups are available
                // on all distributions.
                commandLine = QStringLiteral("/usr/sbin/usermod -aG %1 '%2'")
                                    .arg(group).arg(username).toStdWString();
                WslApi::LaunchInteractive(distName.c_str(), commandLine.c_str(),
                                          TRUE, &exitCode);
            }

            uint32_t uid = WslUtil::getUid(distName, username);
            dist.setDefaultUID(uid);
        }
    } catch (const std::runtime_error &err) {
        QMessageBox::critical(this, QString::null,
                tr("Failed to register distribution: %1").arg(err.what()));
        return;
    }
}
