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

#include "wslwrap.h"

#include <objbase.h>
#include <string>
#include <vector>

class WslDistribution
{
public:
    WslDistribution();

    bool isValid() const { return !m_name.empty(); }

    const std::wstring &name() const { return m_name; }
    const UUID &uuid() const { return m_uuid; }
    std::wstring uuidString() const;

    uint32_t version() const { return m_version; }
    uint32_t defaultUID() const { return m_defaultUID; }
    WslApi::DistributionFlags flags() const { return m_flags; }
    const std::vector<std::wstring> &defaultEnvironment() const { return m_defaultEnvironment; }

    uint32_t state() const { return m_state; }
    const std::wstring &path() const { return m_path; }
    const std::wstring &kernelCmdLine() const { return m_kernelCmdLine; }
    const std::wstring &packageFamilyName() const { return m_packageFamilyName; }

    static WslDistribution loadFromRegistry(const std::wstring &path);

private:
    // Available in WSL API
    std::wstring m_name;
    uint32_t m_version;
    uint32_t m_defaultUID;
    WslApi::DistributionFlags m_flags;
    std::vector<std::wstring> m_defaultEnvironment;

    // Available only in the Registry
    UUID m_uuid;
    uint32_t m_state;
    std::wstring m_path;
    std::wstring m_kernelCmdLine;
    std::wstring m_packageFamilyName;
};

class WslRegistry
{
public:
    WslRegistry();
    ~WslRegistry();

    std::vector<WslDistribution> getDistributions() const;
    WslDistribution defaultDistribution() const;
    uint32_t defaultUID() const;
    uint32_t defaultGID() const;
    std::wstring defaultUsername() const;

    void setDefaultUID(uint32_t uid);
    void setDefaultGID(uint32_t gid);
    void setDefaultUsername(const std::wstring &name);

    WslDistribution findDistByName(const std::wstring &name) const;
    static WslDistribution findDistByUuid(const std::wstring &uuid);

private:
    HKEY m_lxssKey;
};
