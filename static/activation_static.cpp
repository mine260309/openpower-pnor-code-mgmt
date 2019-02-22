#include "activation_static.hpp"

#include "item_updater.hpp"
//#include "serialize.hpp"
//
//#include <experimental/filesystem>
//#include <phosphor-logging/log.hpp>
//#include <sdbusplus/exception.hpp>

namespace openpower
{
namespace software
{
namespace updater
{
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

void ActivationStatic::startActivation()
{
    // Since the squashfs image has not yet been loaded to pnor and the
    // RW volumes have not yet been created, we need to start the
    // service files for each of those actions.

#if 0
    if (!activationProgress)
    {
        activationProgress = std::make_unique<ActivationProgress>(bus, path);
    }

    if (!activationBlocksTransition)
    {
        activationBlocksTransition =
            std::make_unique<ActivationBlocksTransition>(bus, path);
    }

    constexpr auto ubimountService = "obmc-flash-bios-ubimount@";
    auto ubimountServiceFile =
        std::string(ubimountService) + versionId + ".service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(ubimountServiceFile, "replace");
    bus.call_noreply(method);

    activationProgress->progress(10);
#endif
}

void ActivationStatic::unitStateChange(sdbusplus::message::message& msg)
{
#if 0
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    auto ubimountServiceFile =
        "obmc-flash-bios-ubimount@" + versionId + ".service";

    if (newStateUnit == ubimountServiceFile && newStateResult == "done")
    {
        ubiVolumesCreated = true;
        activationProgress->progress(activationProgress->progress() + 50);
    }

    if (ubiVolumesCreated)
    {
        Activation::activation(
            softwareServer::Activation::Activations::Activating);
    }

    if ((newStateUnit == ubimountServiceFile) &&
        (newStateResult == "failed" || newStateResult == "dependency"))
    {
        Activation::activation(softwareServer::Activation::Activations::Failed);
    }

    return;
#endif
}

} // namespace updater
} // namespace software
} // namespace openpower
