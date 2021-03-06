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

#include <QMainWindow>

class WslRegistry;
class WslDistribution;
class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QLineEdit;
class QCheckBox;
class QFrame;
class QAction;

class WslUi : public QMainWindow
{
public:
    WslUi();
    ~WslUi();

    static QIcon pickDistIcon(const QString &name);

private slots:
    void distSelected(QListWidgetItem *current, QListWidgetItem *);
    void distActivated(QListWidgetItem *item);
    void chooseUser(bool);
    void commitDistFlags(bool);
    void commitKernelCmdLine();
    void environSelected(QTreeWidgetItem *current, QTreeWidgetItem *);
    void environChanged(QTreeWidgetItem *item, int column);
    void deleteSelectedEnviron(bool);
    void installDistribution();
    void loadDistributions();
    void setCurrentDistAsDefault();

private:
    WslRegistry *m_registry;
    QListWidget *m_distList;
    QFrame *m_distDetails;
    QLabel *m_name;
    QLabel *m_fsType;
    QLabel *m_defaultUser;
    QLineEdit *m_location;
    QCheckBox *m_enableInterop;
    QCheckBox *m_appendNTPath;
    QCheckBox *m_enableDriveMounting;
    QLineEdit *m_kernelCmdLine;
    QTreeWidget *m_defaultEnvironment;
    QAction *m_envAdd;
    QAction *m_envEdit;
    QAction *m_envDel;

    QAction *m_openShell;
    QAction *m_setDefault;
    QAction *m_installDist;

    QListWidgetItem *findDistByUuid(const QString &uuid);
    void updateDistProperties(const WslDistribution &dist);

    WslDistribution getDistribution(QListWidgetItem *item);
};
