#include "config.h"

#include "msl_verify.hpp"

#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <regex>

namespace openpower
{
namespace software
{
namespace image
{

namespace fs = std::experimental::filesystem;
using namespace phosphor::logging;

int MinimumShipLevel::compare(const Version& a, const Version& b)
{
    if (a.major < b.major)
    {
        return -1;
    }
    else if (a.major > b.major)
    {
        return 1;
    }

    if (a.minor < b.minor)
    {
        return -1;
    }
    else if (a.minor > b.minor)
    {
        return 1;
    }

    if (a.rev < b.rev)
    {
        return -1;
    }
    else if (a.rev > b.rev)
    {
        return 1;
    }

    return 0;
}

void MinimumShipLevel::parse(const std::string& versionStr, Version& version)
{
    std::smatch match;
    version = {0, 0, 0};

    // Match for vX.Y.Z
    std::regex regex{"v([0-9]+)\\.([0-9]+)\\.([0-9]+)", std::regex::extended};

    if (!std::regex_search(versionStr, match, regex))
    {
        // Match for vX.Y
        std::regex regexShort{"v([0-9]+)\\.([0-9]+)", std::regex::extended};
        if (!std::regex_search(versionStr, match, regexShort))
        {
            log<level::ERR>("Unable to parse PNOR version",
                            entry("VERSION=%s", versionStr.c_str()));
            return;
        }
    }
    else
    {
        // Populate Z
        version.rev = std::stoi(match[3]);
    }
    version.major = std::stoi(match[1]);
    version.minor = std::stoi(match[2]);
}

std::string MinimumShipLevel::getFunctionalVersion()
{
    if (!fs::exists(PNOR_RO_ACTIVE_PATH))
    {
        return {};
    }

    fs::path versionPath(PNOR_RO_ACTIVE_PATH);
    versionPath /= PNOR_VERSION_PARTITION;
    if (!fs::is_regular_file(versionPath))
    {
        return {};
    }

    std::ifstream versionFile(versionPath);
    std::string versionStr;
    std::getline(versionFile, versionStr);

    return versionStr;
}

bool MinimumShipLevel::verify()
{
    if (minShipLevel.empty())
    {
        return true;
    }

    auto actual = getFunctionalVersion();
    if (actual.empty())
    {
        return true;
    }

    // Multiple min versions separated by a space can be specified, parse them
    // into a vector, then sort them in ascending order
    std::istringstream minStream(minShipLevel);
    std::vector<std::string> mins(std::istream_iterator<std::string>{minStream},
                                  std::istream_iterator<std::string>());
    std::sort(mins.begin(), mins.end());

    // In order to handle non-continuous multiple min versions, need to compare
    // the major.minor section first, then if they're the same, compare the rev.
    // Ex: the min versions specified are 2.0.10 and 2.2. We need to pass if
    // actual is 2.0.11 but fail if it's 2.1.x.
    // 1. Save off the rev number to compare later if needed.
    // 2. Zero out the rev number to just compare major and minor.
    Version actualVersion = {0, 0, 0};
    parse(actual, actualVersion);
    Version actualRev = {0, 0, actualVersion.rev};
    actualVersion.rev = 0;

    auto rc = 0;
    std::string tmpMin{};

    for (auto const& min : mins)
    {
        tmpMin = min;

        Version minVersion = {0, 0, 0};
        parse(min, minVersion);
        Version minRev = {0, 0, minVersion.rev};
        minVersion.rev = 0;

        rc = compare(actualVersion, minVersion);
        if (rc < 0)
        {
            break;
        }
        else if (rc == 0)
        {
            // Same major.minor version, compare the rev
            rc = compare(actualRev, minRev);
            break;
        }
    }
    if (rc < 0)
    {
        log<level::ERR>(
            "PNOR Mininum Ship Level NOT met",
            entry("MIN_VERSION=%s", tmpMin.c_str()),
            entry("ACTUAL_VERSION=%s", actual.c_str()),
            entry("VERSION_PURPOSE=%s",
                  "xyz.openbmc_project.Software.Version.VersionPurpose.Host"));
        return false;
    }

    return true;
}

} // namespace image
} // namespace software
} // namespace openpower
