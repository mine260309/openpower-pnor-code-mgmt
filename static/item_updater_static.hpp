#pragma once

#include "item_updater.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

/** @class ItemUpdaterStatic
 *  @brief Manages the activation of the host version items for static layout
 */
class ItemUpdaterStatic : public ItemUpdater
{
  public:
    ItemUpdaterStatic(sdbusplus::bus::bus& bus, const std::string& path)
      : ItemUpdater(bus, path)
    {
        processPNORImage();
        gardReset = std::make_unique<GardReset>(bus, GARD_PATH);
        volatileEnable = std::make_unique<ObjectEnable>(bus, volatilePath);

        // Emit deferred signal.
        emit_object_added();
    }
    virtual ~ItemUpdaterStatic() {}

    void freePriority(uint8_t value, const std::string& versionId) override;

    bool isLowestPriority(uint8_t value) override;

    void processPNORImage() override;

    void erase(std::string entryId) override;

    void deleteAll() override;

    void freeSpace() override;

    void createActiveAssociation(const std::string& path) override;

    void updateFunctionalAssociation(const std::string& path) override;

    void removeAssociation(const std::string& path) override;

    bool isVersionFunctional(const std::string& versionId) override;

  private:
    /** @brief Callback function for Software.Version match.
     *  @details Creates an Activation D-Bus object.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void createActivation(sdbusplus::message::message& msg) override;

    /** @brief Host factory reset - clears PNOR partitions for each
     * Activation D-Bus object */
    void reset() override;
};

} // namespace updater
} // namespace software
} // namespace openpower
