#pragma once

#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

/** @class Activation
 *  @brief OpenBMC activation software management implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.Activation DBus API.
 */
class ActivationUbi : public Activation
{
  public:
    /** @brief Constructs Activation Software Manager
     *
     * @param[in] bus    - The Dbus bus object
     * @param[in] path   - The Dbus object path
     * @param[in] parent - Parent object.
     * @param[in] versionId  - The software version id
     * @param[in] extVersion - The extended version
     * @param[in] activationStatus - The status of Activation
     * @param[in] assocs - Association objects
     */
    ActivationUbi(sdbusplus::bus::bus& bus, const std::string& path,
               ItemUpdater& parent, std::string& versionId,
               std::string& extVersion,
               sdbusplus::xyz::openbmc_project::Software::server::Activation::
                   Activations activationStatus,
               AssociationList& assocs) :
        Activation(bus, path, parent, versionId, extVersion, activationStatus, assocs)
    {
    }

  private:
    void unitStateChange(sdbusplus::message::message& msg) override;
    void startActivation() override;
};

} // namespace updater
} // namespace software
} // namespace openpower
