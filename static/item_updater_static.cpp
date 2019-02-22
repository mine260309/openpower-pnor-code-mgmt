#include "config.h"

#include "static/item_updater_static.hpp"
#include "static/activation_static.hpp"

#include "serialize.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <queue>
#include <string>
#include <sstream>
#include <xyz/openbmc_project/Software/Version/server.hpp>

namespace utils
{

template<typename... Ts>
std::string concat_string(Ts const&... ts){
    std::stringstream s;
    ((s << ts << " "), ...) << std::endl;
    return s.str();
}

// Helper function to run pflash command
template<typename... Ts>
std::string pflash(Ts const&... ts)
{
    std::array<char, 512> buffer;
    std::string cmd = concat_string("pflash", ts ...);
    std::stringstream result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result << buffer.data();
    }
    return result.str();
}

std::string getPNORVersion()
{
    // Read the VERSION partition skipping the first 4K
    auto r = pflash("-P", "VERSION", "-r", "/dev/stderr", "--skip=4096",
                    "2>&1 > /dev/null");
    return r;
}

}

namespace openpower
{
namespace software
{
namespace updater
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::experimental::filesystem;

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

// TODO: Change paths once openbmc/openbmc#1663 is completed.
constexpr auto MBOXD_INTERFACE = "org.openbmc.mboxd";
constexpr auto MBOXD_PATH = "/org/openbmc/mboxd";

void ItemUpdaterStatic::createActivation(sdbusplus::message::message& m)
{
#if 0
    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;
    namespace msg = sdbusplus::message;
    namespace variant_ns = msg::variant_ns;

    sdbusplus::message::object_path objPath;
    std::map<std::string, std::map<std::string, msg::variant<std::string>>>
        interfaces;
    m.read(objPath, interfaces);

    std::string path(std::move(objPath));
    std::string filePath;
    auto purpose = VersionPurpose::Unknown;
    std::string version;

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    // Only process the Host and System images
                    auto value = SVersion::convertVersionPurposeFromString(
                        variant_ns::get<std::string>(property.second));

                    if (value == VersionPurpose::Host ||
                        value == VersionPurpose::System)
                    {
                        purpose = value;
                    }
                }
                else if (property.first == "Version")
                {
                    version = variant_ns::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = variant_ns::get<std::string>(property.second);
                }
            }
        }
    }
    if ((filePath.empty()) || (purpose == VersionPurpose::Unknown))
    {
        return;
    }

    // Version id is the last item in the path
    auto pos = path.rfind("/");
    if (pos == std::string::npos)
    {
        log<level::ERR>("No version id found in object path",
                        entry("OBJPATH=%s", path.c_str()));
        return;
    }

    auto versionId = path.substr(pos + 1);

    if (activations.find(versionId) == activations.end())
    {
        // Determine the Activation state by processing the given image dir.
        auto activationState = server::Activation::Activations::Invalid;
        AssociationList associations = {};
        if (validateSquashFSImage(filePath) == 0)
        {
            activationState = server::Activation::Activations::Ready;
            // Create an association to the host inventory item
            associations.emplace_back(std::make_tuple(
                ACTIVATION_FWD_ASSOCIATION, ACTIVATION_REV_ASSOCIATION,
                HOST_INVENTORY_PATH));
        }

        fs::path manifestPath(filePath);
        manifestPath /= MANIFEST_FILE;
        std::string extendedVersion =
            (Version::getValue(
                 manifestPath.string(),
                 std::map<std::string, std::string>{{"extended_version", ""}}))
                .begin()
                ->second;

        activations.insert(std::make_pair(
            versionId, std::make_unique<ActivationUbi>(
                           bus, path, *this, versionId, extendedVersion,
                           activationState, associations)));

        auto versionPtr = std::make_unique<Version>(
            bus, path, *this, versionId, version, purpose, filePath,
            std::bind(&ItemUpdaterStatic::erase, this, std::placeholders::_1));
        versionPtr->deleteObject =
            std::make_unique<Delete>(bus, path, *versionPtr);
        versions.insert(std::make_pair(versionId, std::move(versionPtr)));
    }
    return;
#endif
}

void ItemUpdaterStatic::processPNORImage()
{
    // TODO: use pflash to extract PNOR version and calcuate version id
    auto fullVersion = utils::getPNORVersion();

    const auto& [version, extendedVersion] = Version::getVersions(fullVersion);
    auto id = Version::getId(version);

    auto activationState = server::Activation::Activations::Active;
    if (version.empty())
    {
        log<level::ERR>("Failed to read version",
                        entry("VERSION=%s", fullVersion.c_str()));
        activationState = server::Activation::Activations::Invalid;
    }

    if (extendedVersion.empty())
    {
        log<level::ERR>("Failed to read extendedVersion",
                        entry("VERSION=%s", fullVersion.c_str()));
        activationState = server::Activation::Activations::Invalid;
    }

    auto purpose = server::Version::VersionPurpose::Host;
    auto path = fs::path(SOFTWARE_OBJPATH) / id;
    AssociationList associations = {};

    if (activationState == server::Activation::Activations::Active)
    {
        // Create an association to the host inventory item
        associations.emplace_back(std::make_tuple(
            ACTIVATION_FWD_ASSOCIATION, ACTIVATION_REV_ASSOCIATION,
            HOST_INVENTORY_PATH));

        // Create an active association since this image is active
        createActiveAssociation(path);
    }

    // Create Activation instance for this version.
    activations.insert(
        std::make_pair(id, std::make_unique<ActivationStatic>(
                               bus, path, *this, id, extendedVersion,
                               activationState, associations)));

    // If Active, create RedundancyPriority instance for this version.
    if (activationState == server::Activation::Activations::Active)
    {
        // For now only one PNOR is supported with static layout
        activations.find(id)->second->redundancyPriority =
            std::make_unique<RedundancyPriority>(
                bus, path, *(activations.find(id)->second), 0);
    }

    // Create Version instance for this version.
    auto versionPtr = std::make_unique<Version>(
        bus, path, *this, id, version, purpose, "",
        std::bind(&ItemUpdaterStatic::erase, this, std::placeholders::_1));
    versionPtr->deleteObject =
        std::make_unique<Delete>(bus, path, *versionPtr);
    versions.insert(std::make_pair(id, std::move(versionPtr)));

    if (!id.empty())
    {
        updateFunctionalAssociation(std::string{SOFTWARE_OBJPATH} + '/' + id);
    }
}

void ItemUpdaterStatic::reset()
{
#if 0
    std::vector<uint8_t> mboxdArgs;

    // Suspend mboxd - no args required.
    auto dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH,
                                        MBOXD_INTERFACE, "cmd");

    dbusCall.append(static_cast<uint8_t>(3), mboxdArgs);

    auto responseMsg = bus.call(dbusCall);
    if (responseMsg.is_method_error())
    {
        log<level::ERR>("Error in mboxd suspend call");
        elog<InternalFailure>();
    }

    constexpr static auto patchDir = "/usr/local/share/pnor";
    if (fs::is_directory(patchDir))
    {
        for (const auto& iter : fs::directory_iterator(patchDir))
        {
            fs::remove_all(iter);
        }
    }

    // Clear the read-write partitions.
    for (const auto& it : activations)
    {
        auto rwDir = PNOR_RW_PREFIX + it.first;
        if (fs::is_directory(rwDir))
        {
            for (const auto& iter : fs::directory_iterator(rwDir))
            {
                fs::remove_all(iter);
            }
        }
    }

    // Clear the preserved partition.
    if (fs::is_directory(PNOR_PRSV))
    {
        for (const auto& iter : fs::directory_iterator(PNOR_PRSV))
        {
            fs::remove_all(iter);
        }
    }

    // Resume mboxd with arg 1, indicating that the flash was modified.
    dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH, MBOXD_INTERFACE,
                                   "cmd");

    mboxdArgs.push_back(1);
    dbusCall.append(static_cast<uint8_t>(4), mboxdArgs);

    responseMsg = bus.call(dbusCall);
    if (responseMsg.is_method_error())
    {
        log<level::ERR>("Error in mboxd resume call");
        elog<InternalFailure>();
    }

    return;
#endif
}

bool ItemUpdaterStatic::isVersionFunctional(const std::string& versionId)
{
#if 0
    if (!fs::exists(PNOR_RO_ACTIVE_PATH))
    {
        return false;
    }

    fs::path activeRO = fs::read_symlink(PNOR_RO_ACTIVE_PATH);

    if (!fs::is_directory(activeRO))
    {
        return false;
    }

    if (activeRO.string().find(versionId) == std::string::npos)
    {
        return false;
    }

    // active PNOR is the version we're checking
    return true;
#endif
    return true;
}

void ItemUpdaterStatic::freePriority(uint8_t value, const std::string& versionId)
{
#if 0
    // TODO openbmc/openbmc#1896 Improve the performance of this function
    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() == value &&
                intf.second->versionId != versionId)
            {
                intf.second->redundancyPriority.get()->priority(value + 1);
            }
        }
    }
#endif
}

bool ItemUpdaterStatic::isLowestPriority(uint8_t value)
{
#if 0
    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() < value)
            {
                return false;
            }
        }
    }
#endif
    return true;
}

void ItemUpdaterStatic::erase(std::string entryId)
{
#if 0
    if (isVersionFunctional(entryId) && isChassisOn())
    {
        log<level::ERR>(("Error: Version " + entryId +
                         " is currently active and running on the host."
                         " Unable to remove.")
                            .c_str());
        return;
    }
    // Remove priority persistence file
    removeFile(entryId);

    // Removing read-only and read-write partitions
    removeReadWritePartition(entryId);
    removeReadOnlyPartition(entryId);

    // Removing entry in versions map
    auto it = versions.find(entryId);
    if (it == versions.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId +
                         " in item updater versions map."
                         " Unable to remove.")
                            .c_str());
    }
    else
    {
        versions.erase(entryId);
    }

    // Removing entry in activations map
    auto ita = activations.find(entryId);
    if (ita == activations.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId +
                         " in item updater activations map."
                         " Unable to remove.")
                            .c_str());
    }
    else
    {
        removeAssociation(ita->second->path);
        activations.erase(entryId);
    }
    return;
#endif
}

void ItemUpdaterStatic::deleteAll()
{
#if 0
    auto chassisOn = isChassisOn();

    for (const auto& activationIt : activations)
    {
        if (isVersionFunctional(activationIt.first) && chassisOn)
        {
            continue;
        }
        else
        {
            ItemUpdaterStatic::erase(activationIt.first);
        }
    }

    // Remove any remaining pnor-ro- or pnor-rw- volumes that do not match
    // the current version.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("obmc-flash-bios-cleanup.service", "replace");
    bus.call_noreply(method);
#endif
}

// TODO: openbmc/openbmc#1402 Monitor flash usage
void ItemUpdaterStatic::freeSpace()
{
#if 0
    //  Versions with the highest priority in front
    std::priority_queue<std::pair<int, std::string>,
                        std::vector<std::pair<int, std::string>>,
                        std::less<std::pair<int, std::string>>>
        versionsPQ;

    std::size_t count = 0;
    for (const auto& iter : activations)
    {
        if (iter.second.get()->activation() ==
            server::Activation::Activations::Active)
        {
            count++;
            // Don't put the functional version on the queue since we can't
            // remove the "running" PNOR version if it allows multiple PNORs
            // But removing functional version if there is only one PNOR.
            if (ACTIVE_PNOR_MAX_ALLOWED > 1 &&
                isVersionFunctional(iter.second->versionId))
            {
                continue;
            }
            versionsPQ.push(std::make_pair(
                iter.second->redundancyPriority.get()->priority(),
                iter.second->versionId));
        }
    }

    // If the number of PNOR versions is over ACTIVE_PNOR_MAX_ALLOWED -1,
    // remove the highest priority one(s).
    while ((count >= ACTIVE_PNOR_MAX_ALLOWED) && (!versionsPQ.empty()))
    {
        erase(versionsPQ.top().second);
        versionsPQ.pop();
        count--;
    }
#endif
}

void ItemUpdaterStatic::createActiveAssociation(const std::string& path)
{
    assocs.emplace_back(
        std::make_tuple(ACTIVE_FWD_ASSOCIATION, ACTIVE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdaterStatic::updateFunctionalAssociation(const std::string& path)
{
    // remove all functional associations
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<0>(*iter)).compare(FUNCTIONAL_FWD_ASSOCIATION) == 0)
        {
            iter = assocs.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    assocs.emplace_back(std::make_tuple(FUNCTIONAL_FWD_ASSOCIATION,
                                        FUNCTIONAL_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdaterStatic::removeAssociation(const std::string& path)
{
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<2>(*iter)).compare(path) == 0)
        {
            iter = assocs.erase(iter);
            associations(assocs);
        }
        else
        {
            ++iter;
        }
    }
}

std::string ItemUpdater::determineId(const std::string& symlinkPath)
{
#if 0
    if (!fs::exists(symlinkPath))
    {
        return {};
    }

    auto target = fs::canonical(symlinkPath).string();

    // check to make sure the target really exists
    if (!fs::is_regular_file(target + "/" + PNOR_TOC_FILE))
    {
        return {};
    }
    // Get the image <id> from the symlink target
    // for example /media/ro-2a1022fe
    static const auto PNOR_RO_PREFIX_LEN = strlen(PNOR_RO_PREFIX);
    return target.substr(PNOR_RO_PREFIX_LEN);
#endif
    return "";
}

void GardReset::reset()
{
#if 0
    // The GARD partition is currently misspelled "GUARD." This file path will
    // need to be updated in the future.
    auto path = fs::path(PNOR_PRSV_ACTIVE_PATH);
    path /= "GUARD";
    std::vector<uint8_t> mboxdArgs;

    auto dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH,
                                        MBOXD_INTERFACE, "cmd");

    // Suspend mboxd - no args required.
    dbusCall.append(static_cast<uint8_t>(3), mboxdArgs);

    auto responseMsg = bus.call(dbusCall);
    if (responseMsg.is_method_error())
    {
        log<level::ERR>("Error in mboxd suspend call");
        elog<InternalFailure>();
    }

    if (fs::is_regular_file(path))
    {
        fs::remove(path);
    }

    dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH, MBOXD_INTERFACE,
                                   "cmd");

    // Resume mboxd with arg 1, indicating that the flash is modified.
    mboxdArgs.push_back(1);
    dbusCall.append(static_cast<uint8_t>(4), mboxdArgs);

    responseMsg = bus.call(dbusCall);
    if (responseMsg.is_method_error())
    {
        log<level::ERR>("Error in mboxd resume call");
        elog<InternalFailure>();
    }
#endif
}

} // namespace updater
} // namespace software
} // namespace openpower
