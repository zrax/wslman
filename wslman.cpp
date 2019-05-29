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

#include "wslregistry.h"

#include <locale>
#include <iostream>

int main(int argc, char *argv[])
{
    std::locale("");

    try {
        WslRegistry registry;
        for (const auto &dist : registry.getDistributions()) {
            std::wcout << std::endl;
            std::wcout << dist.name() << L":" << std::endl;
            std::wcout << L"    UUID:           " << dist.uuidString() << std::endl;
            std::wcout << L"    Version:        " << dist.version() << std::endl;
            std::wcout << L"    Default UID:    " << dist.defaultUID() << std::endl;
            std::wcout << L"    Flags:          " << static_cast<uint32_t>(dist.flags()) << std::endl;
            std::wcout << L"    State:          " << dist.state() << std::endl;
            std::wcout << L"    Base Path:      " << dist.path() << std::endl;
            std::wcout << L"    Kernel Cmdline: " << dist.kernelCmdLine() << std::endl;
            std::wcout << L"    Package Family: " << dist.packageFamilyName() << std::endl;
            std::wcout << L"    Environment:" << std::endl;
            for (const auto &env : dist.defaultEnvironment()) {
                std::wcout << L"        " << env << std::endl;
            }
        }
    } catch (const std::runtime_error &err) {
        std::wcout << L"Exception: " << err.what() << std::endl;
    }

    return 0;
}
