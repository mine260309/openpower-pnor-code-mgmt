#include "config.h"

#include "item_updater.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

namespace openpower
{
namespace software
{
namespace updater
{
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::filesystem;

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

void ItemUpdater::createActivation(sdbusplus::message::message& m)
{
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
        if (validateImage(filePath))
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

        auto activation = createActivationObject(
                path, versionId, extendedVersion, activationState,
                associations);
        activations.emplace(versionId, std::move(activation));

        auto versionPtr = createVersionObject(path, versionId, version, purpose, filePath);
        versions.emplace(versionId, std::move(versionPtr));
    }
    return;
}

bool ItemUpdater::isChassisOn()
{
    auto mapperCall = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetObject");

    mapperCall.append(CHASSIS_STATE_PATH,
                      std::vector<std::string>({CHASSIS_STATE_OBJ}));
    auto mapperResponseMsg = bus.call(mapperCall);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in Mapper call");
        elog<InternalFailure>();
    }
    using MapperResponseType = std::map<std::string, std::vector<std::string>>;
    MapperResponseType mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        log<level::ERR>("Invalid Response from mapper");
        elog<InternalFailure>();
    }

    auto method = bus.new_method_call((mapperResponse.begin()->first).c_str(),
                                      CHASSIS_STATE_PATH,
                                      SYSTEMD_PROPERTY_INTERFACE, "Get");
    method.append(CHASSIS_STATE_OBJ, "CurrentPowerState");
    auto response = bus.call(method);
    if (response.is_method_error())
    {
        log<level::ERR>("Error in fetching current Chassis State",
                        entry("MAPPERRESPONSE=%s",
                              (mapperResponse.begin()->first).c_str()));
        elog<InternalFailure>();
    }
    sdbusplus::message::variant<std::string> currentChassisState;
    response.read(currentChassisState);
    auto strParam =
        sdbusplus::message::variant_ns::get<std::string>(currentChassisState);
    return (strParam != CHASSIS_STATE_OFF);
}

} // namespace updater
} // namespace software
} // namespace openpower
