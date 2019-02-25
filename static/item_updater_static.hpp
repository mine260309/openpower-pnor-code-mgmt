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

    void processPNORImage() override;

    void erase(std::string entryId) override;

    void deleteAll() override;

    void freeSpace() override;

    void createActiveAssociation(const std::string& path) override;

    void updateFunctionalAssociation(const std::string& id) override;

    void removeAssociation(const std::string& path) override;

    bool isVersionFunctional(const std::string& versionId) override;

  private:

    std::unique_ptr<Activation> createActivationObject(
            const std::string& path,
            const std::string& versionId,
            const std::string& extVersion,
            sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations activationStatus,
            AssociationList& assocs) override;

    std::unique_ptr<Version> createVersionObject(
            const std::string& objPath,
            const std::string& versionId,
            const std::string& versionString,
            sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose versionPurpose,
            const std::string& filePath) override;

    bool validateImage(const std::string& path) override;

    /** @brief Host factory reset - clears PNOR partitions for each
     * Activation D-Bus object */
    void reset() override;

    /** @brief The functional version ID */
    std::string functionalVersionId;
};

} // namespace updater
} // namespace software
} // namespace openpower
