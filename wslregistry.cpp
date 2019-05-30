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

#define LXSS_ROOT_PATH L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss"

static std::wstring winregGetWstring(const std::wstring &path, LPCWSTR name)
{
    DWORD length;
    auto rc = RegGetValueW(HKEY_CURRENT_USER, path.c_str(), name, RRF_RT_REG_SZ,
                           nullptr, nullptr, &length);
    if (rc != ERROR_SUCCESS)
        return std::wstring();

    auto buffer = std::make_unique<wchar_t[]>(length);
    rc = RegGetValueW(HKEY_CURRENT_USER, path.c_str(), name, RRF_RT_REG_SZ,
                      nullptr, buffer.get(), &length);
    if (rc != ERROR_SUCCESS)
        return std::wstring();

    return std::wstring(buffer.get());
}

static std::vector<std::wstring>
winregGetWstringArray(const std::wstring &path, LPCWSTR name)
{
    DWORD length;
    auto rc = RegGetValueW(HKEY_CURRENT_USER, path.c_str(), name, RRF_RT_REG_MULTI_SZ,
                           nullptr, nullptr, &length);
    if (rc != ERROR_SUCCESS)
        return {};

    auto buffer = std::make_unique<wchar_t[]>(length);
    rc = RegGetValueW(HKEY_CURRENT_USER, path.c_str(), name, RRF_RT_REG_MULTI_SZ,
                      nullptr, buffer.get(), &length);
    if (rc != ERROR_SUCCESS)
        return {};

    std::vector<std::wstring> result;
    wchar_t *bufp = buffer.get();
    for ( ;; ) {
        auto slen = wcslen(bufp);
        if (slen == 0)
            break;

        result.emplace_back(bufp, slen);
        bufp += slen + 1;
    }

    return result;
}

static uint32_t winregGetDword(const std::wstring &path, LPCWSTR name)
{
    DWORD value;
    DWORD length = sizeof(value);
    auto rc = RegGetValueW(HKEY_CURRENT_USER, path.c_str(), name, RRF_RT_REG_DWORD,
                           nullptr, &value, &length);
    if (rc != ERROR_SUCCESS)
        return 0;

    return static_cast<uint32_t>(value);
}

static bool winregSetWstring(const std::wstring &path, LPCWSTR name,
                             const std::wstring &value)
{
    auto size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    auto rc = RegSetKeyValueW(HKEY_CURRENT_USER, path.c_str(), name, REG_SZ,
                              value.c_str(), size);
    return rc == ERROR_SUCCESS;
}

static bool winregSetWstringArray(const std::wstring &path, LPCWSTR name,
                                  const std::vector<std::wstring> &value)
{
    size_t length = 1;
    for (const std::wstring &str : value)
        length += str.size() + 1;

    auto buffer = std::make_unique<wchar_t[]>(length);
    wchar_t *bufp = buffer.get();
    for (const std::wstring &str : value) {
        std::char_traits<wchar_t>::copy(bufp, str.c_str(), str.size());
        bufp += str.size();
        *bufp++ = 0;
    }
    *bufp = 0;

    auto size = static_cast<DWORD>(length * sizeof(wchar_t));
    auto rc = RegSetKeyValueW(HKEY_CURRENT_USER, path.c_str(), name, REG_MULTI_SZ,
                              buffer.get(), size);
    return rc == ERROR_SUCCESS;
}

static bool winregSetDword(const std::wstring &path, LPCWSTR name, uint32_t value)
{
    DWORD dwValue = static_cast<DWORD>(value);
    auto rc = RegSetKeyValueW(HKEY_CURRENT_USER, path.c_str(), name, REG_DWORD,
                              &dwValue, sizeof(value));
    return rc == ERROR_SUCCESS;
}


WslDistribution::WslDistribution()
    : m_version(), m_defaultUID(), m_flags(), m_state()
{
    memset(&m_uuid, 0, sizeof(m_uuid));
}

void WslDistribution::setName(const std::wstring &name)
{
    if (!isValid())
        throw std::runtime_error("Cannot set properties on invalid distributions");

    if (winregSetWstring(LXSS_ROOT_PATH L"\\" + m_uuid, L"DistributionName", name))
        m_name = name;
}

void WslDistribution::setVersion(uint32_t version)
{
    if (!isValid())
        throw std::runtime_error("Cannot set properties on invalid distributions");

    if (winregSetDword(LXSS_ROOT_PATH L"\\" + m_uuid, L"Version", version))
        m_version = version;
}

void WslDistribution::setDefaultUID(uint32_t uid)
{
    if (!isValid())
        throw std::runtime_error("Cannot set properties on invalid distributions");

    if (winregSetDword(LXSS_ROOT_PATH L"\\" + m_uuid, L"DefaultUid", uid))
        m_defaultUID = uid;
}

void WslDistribution::setFlags(WslApi::DistributionFlags flags)
{
    if (!isValid())
        throw std::runtime_error("Cannot set properties on invalid distributions");

    if (winregSetDword(LXSS_ROOT_PATH L"\\" + m_uuid, L"Flags", static_cast<uint32_t>(flags)))
        m_flags = flags;
}

void WslDistribution::setKernelCmdLine(const std::wstring &cmdline)
{
    if (!isValid())
        throw std::runtime_error("Cannot set properties on invalid distributions");

    if (winregSetWstring(LXSS_ROOT_PATH L"\\" + m_uuid, L"KernelCommandLine", cmdline))
        m_kernelCmdLine = cmdline;
}

void WslDistribution::addEnvironment(const std::wstring &key, const std::wstring &value)
{
    std::vector<std::wstring> newEnv = m_defaultEnvironment;
    const std::wstring keySearch = key + L"=";
    bool found = false;
    for (auto ei = newEnv.begin(); ei != newEnv.end(); ++ei) {
        if (ei->rfind(keySearch, 0) == 0) {
            *ei = keySearch + value;
            found = true;
            break;
        }
    }
    if (!found)
        newEnv.emplace_back(keySearch + value);

    setEnvironment(newEnv);
}

void WslDistribution::delEnvironment(const std::wstring &key)
{
    std::vector<std::wstring> newEnv = m_defaultEnvironment;
    const std::wstring keySearch = key + L"=";
    auto ei = newEnv.begin();
    while (ei != newEnv.end()) {
        if (ei->rfind(keySearch, 0) == 0)
            ei = newEnv.erase(ei);
        else
            ++ei;
    }

    setEnvironment(newEnv);
}

void WslDistribution::setEnvironment(const std::vector<std::wstring> &env)
{
    if (!isValid())
        throw std::runtime_error("Cannot set properties on invalid distributions");

    if (winregSetWstringArray(LXSS_ROOT_PATH L"\\" + m_uuid, L"DefaultEnvironment", env))
        m_defaultEnvironment = env;
}

WslDistribution WslDistribution::loadFromRegistry(const std::wstring &path)
{
    WslDistribution dist;

    auto uuidStart = path.rfind(L'\\');
    if (uuidStart == std::wstring::npos)
        throw std::runtime_error("Invalid registry key format");
    dist.m_uuid = path.substr(uuidStart + 1);

    dist.m_name = winregGetWstring(path, L"DistributionName");
    dist.m_version = winregGetDword(path, L"Version");
    dist.m_defaultUID = winregGetDword(path, L"DefaultUid");
    dist.m_flags = static_cast<WslApi::DistributionFlags>(winregGetDword(path, L"Flags"));
    dist.m_defaultEnvironment = winregGetWstringArray(path, L"DefaultEnvironment");

    dist.m_state = winregGetDword(path, L"State");
    dist.m_path = winregGetWstring(path, L"BasePath");
    dist.m_kernelCmdLine = winregGetWstring(path, L"KernelCommandLine");
    dist.m_packageFamilyName = winregGetWstring(path, L"PackageFamilyName");

    return dist;
}


WslRegistry::WslRegistry()
    : m_lxssKey()
{
    auto rc = RegCreateKeyExW(HKEY_CURRENT_USER, LXSS_ROOT_PATH, 0, nullptr,
                              0, KEY_READ, nullptr, &m_lxssKey, nullptr);
    if (rc != ERROR_SUCCESS)
        throw std::runtime_error("Could not open LXSS registry key");
}

WslRegistry::~WslRegistry()
{
    if (m_lxssKey)
        RegCloseKey(m_lxssKey);
}

std::vector<WslDistribution> WslRegistry::getDistributions() const
{
    std::vector<WslDistribution> result;

    wchar_t buffer[MAX_PATH];
    DWORD idx = 0;
    for ( ;; ) {
        DWORD cchResult = MAX_PATH;
        auto rc = RegEnumKeyExW(m_lxssKey, idx++, buffer, &cchResult, nullptr,
                                nullptr, nullptr, nullptr);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        else if (rc != ERROR_SUCCESS)
            throw std::runtime_error("Could not list distributions");

        auto uuid = std::wstring(buffer, cchResult);
        auto distro = findDistByUuid(uuid);
        if (distro.isValid())
            result.emplace_back(std::move(distro));
    }

    return result;
}

WslDistribution WslRegistry::defaultDistribution() const
{
    std::wstring uuid = winregGetWstring(LXSS_ROOT_PATH, L"DefaultDistribution");
    if (uuid.empty())
        return WslDistribution();
    return findDistByUuid(uuid);
}

WslDistribution WslRegistry::findDistByName(const std::wstring &name) const
{
    wchar_t buffer[MAX_PATH];
    DWORD idx = 0;
    for ( ;; ) {
        DWORD cchResult = MAX_PATH;
        auto rc = RegEnumKeyExW(m_lxssKey, idx++, buffer, &cchResult, nullptr,
                                nullptr, nullptr, nullptr);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        else if (rc != ERROR_SUCCESS)
            throw std::runtime_error("Could not list distributions");

        auto uuid = std::wstring(buffer, cchResult);
        std::wstring regPath = LXSS_ROOT_PATH L"\\" + uuid;
        auto distroName = winregGetWstring(regPath, L"DistributionName");
        if (distroName == name)
            return WslDistribution::loadFromRegistry(regPath);
    }

    return WslDistribution();
}

WslDistribution WslRegistry::findDistByUuid(const std::wstring &uuid)
{
    return WslDistribution::loadFromRegistry(LXSS_ROOT_PATH L"\\" + uuid);
}
