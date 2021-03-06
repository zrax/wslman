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

#pragma once

#include <QDialog>
#include <QIcon>

class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QGroupBox;

class WslInstallDialog : public QDialog
{
public:
    WslInstallDialog(QWidget *parent = nullptr);

    bool validate();
    void performInstall();

private:
    QLineEdit *m_distName;
    QIcon m_distIcon;
    QLabel *m_distIconLabel;
    QLineEdit *m_tarball;
    QLineEdit *m_installPath;

    QGroupBox *m_runCmdGroupBox;
    QPlainTextEdit *m_runCommands;

    QGroupBox *m_userGroupBox;
    QLineEdit *m_defaultUsername;
    QLineEdit *m_userGecos;
    QLineEdit *m_userGroups;

    void setupDistribution();
};
