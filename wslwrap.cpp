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

#include <wslapi.h>

typedef HRESULT(STDAPICALLTYPE *WslConfigureDistribution_fn)(PCWSTR, ULONG,
                                    WSL_DISTRIBUTION_FLAGS);
typedef HRESULT(STDAPICALLTYPE *WslGetDistributionConfiguration_fn)(PCWSTR, ULONG *,
                                    ULONG *, WSL_DISTRIBUTION_FLAGS *, PSTR **, ULONG *);
typedef BOOL(STDAPICALLTYPE *WslIsDistributionRegistered_fn)(PCWSTR);
typedef HRESULT(STDAPICALLTYPE *WslLaunch_fn)(PCWSTR, PCWSTR, BOOL, HANDLE, HANDLE,
                                    HANDLE, HANDLE *);
typedef HRESULT(STDAPICALLTYPE *WslLaunchInteractive_fn)(PCWSTR, PCWSTR, BOOL, DWORD *);
typedef HRESULT(STDAPICALLTYPE *WslRegisterDistribution_fn)(PCWSTR, PCWSTR);
typedef HRESULT(STDAPICALLTYPE *WslUnregisterDistribution_fn)(PCWSTR);

#define BIND_API(fname, hmodule) \
    m_##fname = reinterpret_cast<fname##_fn>(GetProcAddress(hmodule, #fname))

struct WslApiBind
{
    HMODULE m_wslAPIDll;

    WslConfigureDistribution_fn m_WslConfigureDistribution;
    WslGetDistributionConfiguration_fn m_WslGetDistributionConfiguration;
    WslIsDistributionRegistered_fn m_WslIsDistributionRegistered;
    WslLaunch_fn m_WslLaunch;
    WslLaunchInteractive_fn m_WslLaunchInteractive;
    WslRegisterDistribution_fn m_WslRegisterDistribution;
    WslUnregisterDistribution_fn m_WslUnregisterDistribution;

    WslApiBind()
    {
        m_wslAPIDll = LoadLibraryExW(L"wslapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (m_wslAPIDll) {
            BIND_API(WslConfigureDistribution, m_wslAPIDll);
            BIND_API(WslGetDistributionConfiguration, m_wslAPIDll);
            BIND_API(WslIsDistributionRegistered, m_wslAPIDll);
            BIND_API(WslLaunch, m_wslAPIDll);
            BIND_API(WslLaunchInteractive, m_wslAPIDll);
            BIND_API(WslRegisterDistribution, m_wslAPIDll);
            BIND_API(WslUnregisterDistribution, m_wslAPIDll);
        }
    }

    ~WslApiBind()
    {
        if (m_wslAPIDll)
            FreeLibrary(m_wslAPIDll);
    }

    static WslApiBind *instance()
    {
        static WslApiBind s_instance;
        return &s_instance;
    }
};

HRESULT WslApi::ConfigureDistribution(PCWSTR distributionName, ULONG defaultUID,
            DistributionFlags wslDistributionFlags)
{
    WslApiBind *api = WslApiBind::instance();
    if (api->m_WslConfigureDistribution) {
        return api->m_WslConfigureDistribution(distributionName, defaultUID,
                        static_cast<WSL_DISTRIBUTION_FLAGS>(wslDistributionFlags));
    }
    throw WslApiUnavailable();
}

HRESULT WslApi::GetDistributionConfiguration(PCWSTR distributionName,
            ULONG *distributionVersion, ULONG *defaultUID,
            DistributionFlags *wslDistributionFlags,
            PSTR **defaultEnvironmentVariables,
            ULONG *defaultEnvironmentVariableCount)
{
    WslApiBind *api = WslApiBind::instance();
    if (api->m_WslGetDistributionConfiguration) {
        WSL_DISTRIBUTION_FLAGS rFlags;
        auto result = api->m_WslGetDistributionConfiguration(distributionName,
                        distributionVersion, defaultUID, &rFlags,
                        defaultEnvironmentVariables, defaultEnvironmentVariableCount);
        if (wslDistributionFlags)
            *wslDistributionFlags = static_cast<DistributionFlags>(rFlags);
        return result;
    }
    throw WslApiUnavailable();
}

BOOL WslApi::IsDistributionRegistered(PCWSTR distributionName)
{
    WslApiBind *api = WslApiBind::instance();
    if (api->m_WslIsDistributionRegistered)
        return api->m_WslIsDistributionRegistered(distributionName);
    throw WslApiUnavailable();
}

HRESULT WslApi::Launch(PCWSTR distributionName, PCWSTR command,
            BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut,
            HANDLE stdErr, HANDLE *process)
{
    WslApiBind *api = WslApiBind::instance();
    if (api->m_WslLaunch) {
        return api->m_WslLaunch(distributionName, command, useCurrentWorkingDirectory,
                                stdIn, stdOut, stdErr, process);
    }
    throw WslApiUnavailable();
}

HRESULT WslApi::LaunchInteractive(PCWSTR distributionName, PCWSTR command,
            BOOL useCurrentWorkingDirectory, DWORD *exitCode)
{
    WslApiBind *api = WslApiBind::instance();
    if (api->m_WslLaunchInteractive) {
        return api->m_WslLaunchInteractive(distributionName, command,
                                           useCurrentWorkingDirectory, exitCode);
    }
    throw WslApiUnavailable();
}

HRESULT WslApi::RegisterDistribution(PCWSTR distributionName, PCWSTR tarGzFilename)
{
    WslApiBind *api = WslApiBind::instance();
    if (api->m_WslRegisterDistribution)
        return api->m_WslRegisterDistribution(distributionName, tarGzFilename);
    throw WslApiUnavailable();
}

HRESULT WslApi::UnregisterDistribution(PCWSTR distributionName)
{
    WslApiBind *api = WslApiBind::instance();
    if (api->m_WslUnregisterDistribution)
        return api->m_WslUnregisterDistribution(distributionName);
    throw WslApiUnavailable();
}
