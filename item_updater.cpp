#include "config.h"

#include "item_updater.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

namespace openpower
{
namespace software
{
namespace updater
{
using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;


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
