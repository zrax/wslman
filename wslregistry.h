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

#include <string>
#include <vector>

class WslDistribution
{
public:
    WslDistribution();

    bool isValid() const { return !m_uuid.empty(); }

    const std::wstring &name() const { return m_name; }
    std::wstring uuid() const { return m_uuid; }

    uint32_t version() const { return m_version; }
    uint32_t defaultUID() const { return m_defaultUID; }
    WslApi::DistributionFlags flags() const { return m_flags; }
    const std::vector<std::wstring> &defaultEnvironment() const { return m_defaultEnvironment; }

    uint32_t state() const { return m_state; }
    const std::wstring &path() const { return m_path; }
    const std::wstring &kernelCmdLine() const { return m_kernelCmdLine; }
    const std::wstring &packageFamilyName() const { return m_packageFamilyName; }

    void setName(const std::wstring &name);
    void setVersion(uint32_t version);
    void setDefaultUID(uint32_t uid);
    void setFlags(WslApi::DistributionFlags flags);
    void setKernelCmdLine(const std::wstring &cmdline);

    void addEnvironment(const std::wstring &key, const std::wstring &value);
    void delEnvironment(const std::wstring &key);
    void setEnvironment(const std::vector<std::wstring> &env);

    static WslDistribution loadFromRegistry(const std::wstring &path);

private:
    // Available in WSL API
    std::wstring m_name;
    uint32_t m_version;
    uint32_t m_defaultUID;
    WslApi::DistributionFlags m_flags;
    std::vector<std::wstring> m_defaultEnvironment;

    // Available only in the Registry
    std::wstring m_uuid;
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
    void setDefaultDistribution(const std::wstring &uuid);

    WslDistribution findDistByName(const std::wstring &name) const;
    static WslDistribution findDistByUuid(const std::wstring &uuid);

private:
    HKEY m_lxssKey;
};
