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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "wslui.h"
#include "wslutils.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QObject::tr("WSL Distribution Manager"));

    if (!WslUtil::checkWindowsVersion(WslUtil::Windows1803)) {
        QMessageBox::critical(nullptr, QString::null,
                QObject::tr("This application requires Windows version 1803 or later"));
        return 1;
    }

    WslUi ui;
    ui.show();

    return app.exec();
}
