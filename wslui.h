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

#include <QMainWindow>

class WslRegistry;
class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QAction;

class WslUi : public QMainWindow
{
public:
    WslUi();
    ~WslUi();

private slots:
    void distSelected(QListWidgetItem *current, QListWidgetItem *);
    void distActivated(QListWidgetItem *item);

private:
    WslRegistry *m_registry;
    QListWidget *m_distList;

    QAction *m_openShell;

    QListWidgetItem *findDistByUuid(const QString &uuid);
};
