#include "config.h"

#include "item_updater_static.hpp"
#include "activation_static.hpp"
#include "pnor_utils.hpp"

#include "serialize.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <queue>
#include <string>
#include <tuple>
#include <xyz/openbmc_project/Software/Version/server.hpp>

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
using sdbusplus::exception::SdBusError;

// TODO: Change paths once openbmc/openbmc#1663 is completed.
constexpr auto MBOXD_INTERFACE = "org.openbmc.mboxd";
constexpr auto MBOXD_PATH = "/org/openbmc/mboxd";

std::unique_ptr<Activation> ItemUpdaterStatic::createActivationObject(
            const std::string& path,
            const std::string& versionId,
            const std::string& extVersion,
            sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations activationStatus,
            AssociationList& assocs)
{
    return std::make_unique<ActivationStatic>(
        bus, path, *this, versionId, extVersion,
                           activationStatus, assocs);
}

std::unique_ptr<Version> ItemUpdaterStatic::createVersionObject(
            const std::string& objPath,
            const std::string& versionId,
            const std::string& versionString,
            sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose versionPurpose,
            const std::string& filePath)
{
    auto version = std::make_unique<Version>(
        bus, objPath, *this, versionId, versionString, versionPurpose, filePath,
        std::bind(&ItemUpdaterStatic::erase, this, std::placeholders::_1));
    version->deleteObject =
        std::make_unique<Delete>(bus, objPath, *version);
    return version;
}

bool ItemUpdaterStatic::validateImage(const std::string& path)
{
    // There is no need to validate static layout pnor
    return true;
}

void ItemUpdaterStatic::processPNORImage()
{
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
        updateFunctionalAssociation(id);
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
    return versionId == functionalVersionId;
}

void ItemUpdaterStatic::freePriority(uint8_t value, const std::string& versionId)
{
    // No need to support priorty for now
}

void ItemUpdaterStatic::erase(std::string entryId)
{
    if (isVersionFunctional(entryId) && isChassisOn())
    {
        log<level::ERR>(("Error: Version " + entryId +
                         " is currently active and running on the host."
                         " Unable to remove.")
                            .c_str());
        // TODO: make it return false to indicate it's not erased
        return;
    }

    // TODO: Should we really erase the PNOR here, or just let it erased by
    // the new activation?

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
}

void ItemUpdaterStatic::deleteAll()
{
    // TODO: This function seems not used, should we delete?
}

void ItemUpdaterStatic::freeSpace()
{
    // For now assume static layout only has 1 active PNOR,
    // so erase the active PNOR
    for (const auto& iter : activations)
    {
        if (iter.second.get()->activation() ==
            server::Activation::Activations::Active)
        {
            erase(iter.second->versionId);
            break;
        }
    }
}

void ItemUpdaterStatic::createActiveAssociation(const std::string& path)
{
    assocs.emplace_back(
        std::make_tuple(ACTIVE_FWD_ASSOCIATION, ACTIVE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdaterStatic::updateFunctionalAssociation(const std::string& id)
{
    std::string path = std::string{SOFTWARE_OBJPATH} + '/' + id;
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
    functionalVersionId = id;
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

void GardReset::reset()
{
    // Clear gard partition
    std::vector<uint8_t> mboxdArgs;
    int rc;

    auto dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH,
                                        MBOXD_INTERFACE, "cmd");
    // Suspend mboxd - no args required.
    dbusCall.append(static_cast<uint8_t>(3), mboxdArgs);

    try
    {
        bus.call_noreply(dbusCall);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in mboxd suspend call",
                entry("ERROR=%s", e.what()));
        elog<InternalFailure>();
    }

    // Clear guard partition
    std::tie(rc, std::ignore) = utils::pflash("-P GUARD -c -f >/dev/null");
    if (rc != 0)
    {
        log<level::ERR>("Failed to clear GUARD",
                entry("RETURNCODE=%d", rc));
    }
    else
    {
        log<level::INFO>("Clear GUARD successfully");
    }

    dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH, MBOXD_INTERFACE,
                                   "cmd");
    // Resume mboxd with arg 1, indicating that the flash is modified.
    mboxdArgs.push_back(1);
    dbusCall.append(static_cast<uint8_t>(4), mboxdArgs);

    try
    {
        bus.call_noreply(dbusCall);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in mboxd resume call",
                entry("ERROR=%s", e.what()));
        elog<InternalFailure>();
    }
}

} // namespace updater
} // namespace software
} // namespace openpower
