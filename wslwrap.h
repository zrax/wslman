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
#include <stdexcept>

class WslApiUnavailable : public std::runtime_error
{
public:
    WslApiUnavailable() : std::runtime_error("WSL API Unavailable") { }
};

namespace WslApi
{
    enum DistributionFlags
    {
        DistributionFlags_None = 0x0,
        DistributionFlags_EnableInterop = 0x1,
        DistributionFlags_AppendNTPath = 0x2,
        DistributionFlags_EnableDriveMounting = 0x4,
        DistributionFlags_KnownMask = 0x7,
    };

    HRESULT ConfigureDistribution(PCWSTR distributionName, ULONG defaultUID,
                DistributionFlags wslDistributionFlags);
    HRESULT GetDistributionConfiguration(PCWSTR distributionName,
                ULONG *distributionVersion, ULONG *defaultUID,
                DistributionFlags *wslDistributionFlags,
                PSTR **defaultEnvironmentVariables,
                ULONG *defaultEnvironmentVariableCount);
    BOOL IsDistributionRegistered(PCWSTR distributionName);
    HRESULT Launch(PCWSTR distributionName, PCWSTR command,
                BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut,
                HANDLE stdErr, HANDLE *process);
    HRESULT LaunchInteractive(PCWSTR distributionName, PCWSTR command,
                BOOL useCurrentWorkingDirectory, DWORD *exitCode);
    HRESULT RegisterDistribution(PCWSTR distributionName, PCWSTR tarGzFilename);
    HRESULT UnregisterDistribution(PCWSTR distributionName);
};
