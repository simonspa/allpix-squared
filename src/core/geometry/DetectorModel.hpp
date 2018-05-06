/**
 * @file
 * @brief Base of detector models
 *
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied
 * verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities
 * granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

/**
 * @defgroup DetectorModels Detector models
 * @brief Collection of detector models supported by the framework
 */

#ifndef ALLPIX_DETECTOR_MODEL_H
#define ALLPIX_DETECTOR_MODEL_H

#include <array>
#include <string>
#include <utility>

#include <Math/Point2D.h>
#include <Math/Point3D.h>
#include <Math/Vector2D.h>
#include <Math/Vector3D.h>

#include "core/config/ConfigReader.hpp"
#include "core/config/exceptions.h"
#include "core/utils/log.h"
#include "tools/ROOT.h"

namespace allpix {
/**
 * @ingroup DetectorModels
 * @brief Base of all detector models
 *
 * Implements the minimum required for a detector model. A model always has a
 * pixel grid with a specific pixel size. The
 * pixel grid defines the base size of the sensor, chip and support. Excess
 * length can be specified. Every part of the
 * detector model has a defined center and size which can be overloaded by
 * specialized detector models. The basic
 * detector model also defines the rotation center in the local coordinate
 * system.
 */
class DetectorModel {
public:
  /**
   * @brief Helper class to hold support layers for a detector model
   */
  class SupportLayer {
    friend class DetectorModel;
    // FIXME Friending this class is broken
    friend class HybridPixelDetectorModel;

  public:
    /**
     * @brief Get the center of the support layer
     * @return Center of the support layer
     */
    ROOT::Math::XYZPoint getCenter() const { return center_; }
    /**
     * @brief Get the size of the support layer
     * @return Size of the support layer
     */
    ROOT::Math::XYZVector getSize() const { return size_; }
    /**
     * @brief Get the material of the support layer
     * @return Support material
     */
    std::string getMaterial() const { return material_; }
    /**
     * @brief Return if the support layer contains a hole
     * @return True if the support layer has a hole, false otherwise
     */
    bool hasHole() { return hole_size_.x() > 1e-9 && hole_size_.y() > 1e-9; }
    /**
     * @brief Get the center of the hole in the support layer
     * @return Center of the hole
     */
    ROOT::Math::XYZPoint getHoleCenter() const {
      return center_ +
             ROOT::Math::XYZVector(hole_offset_.x(), hole_offset_.y(), 0);
    }
    /**
     * @brief Get the full size of the hole in the support layer
     * @return Size of the hole
     */
    ROOT::Math::XYZVector getHoleSize() const { return hole_size_; }

  private:
    /**
     * @brief Constructs a support layer, used in \ref
     * DetectorModel::addSupportLayer
     * @param size Size of the support layer
     * @param offset Offset of the support layer from the center
     * @param material Material of the support layer
     * @param location Location of the support material
     * @param hole_size Size of an optional hole (zero vector if no hole)
     * @param hole_offset Offset of the optional hole from the center of the
     * support layer
     */
    SupportLayer(ROOT::Math::XYZVector size, ROOT::Math::XYZVector offset,
                 std::string material, std::string location,
                 ROOT::Math::XYZVector hole_size,
                 ROOT::Math::XYVector hole_offset)
        : size_(std::move(size)), material_(std::move(material)),
          hole_size_(std::move(hole_size)), offset_(std::move(offset)),
          hole_offset_(std::move(hole_offset)), location_(std::move(location)) {
    }

    // Actual parameters returned
    ROOT::Math::XYZPoint center_;
    ROOT::Math::XYZVector size_;
    std::string material_;
    ROOT::Math::XYZVector hole_size_;

    // Internal parameters to calculate return parameters
    ROOT::Math::XYZVector offset_;
    ROOT::Math::XYVector hole_offset_;
    std::string location_;
  };

  /**
   * @brief Constructs the base detector model
   * @param type Name of the model type
   * @param reader Configuration reader with description of the model
   */
  explicit DetectorModel(std::string type, ConfigReader reader)
      : type_(std::move(type)), reader_(std::move(reader)) {
    using namespace ROOT::Math;
    auto config = reader_.getHeaderConfiguration();

    // Number of pixels
    setNPixels(
        config.get<DisplacementVector2D<Cartesian2D<int>>>("number_of_pixels"));
    // Size of the pixels
    setPixelSize(config.get<XYVector>("pixel_size"));

    // Sensor thickness
    setSensorThickness(config.get<double>("sensor_thickness"));
    // Excess around the sensor from the pixel grid
    auto default_sensor_excess = config.get<double>("sensor_excess", 0);
    setSensorExcessTop(
        config.get<double>("sensor_excess_top", default_sensor_excess));
    setSensorExcessBottom(
        config.get<double>("sensor_excess_bottom", default_sensor_excess));
    setSensorExcessLeft(
        config.get<double>("sensor_excess_left", default_sensor_excess));
    setSensorExcessRight(
        config.get<double>("sensor_excess_right", default_sensor_excess));

    // Chip thickness
    setChipThickness(config.get<double>("chip_thickness", 0));

    // Read support layers
    for (auto &support_config : reader_.getConfigurations("support")) {
      auto thickness = support_config.get<double>("thickness");
      auto size = support_config.get<XYVector>("size");
      auto location = support_config.get<std::string>("location", "chip");
      std::transform(location.begin(), location.end(), location.begin(),
                     ::tolower);
      if (location != "sensor" && location != "chip" &&
          location != "absolute") {
        throw InvalidValueError(
            support_config, "location",
            "location of the support should be 'chip', 'sensor' or 'absolute'");
      }
      XYZVector offset;
      if (location == "absolute") {
        offset = support_config.get<XYZVector>("offset");
      } else {
        auto xy_offset = support_config.get<XYVector>("offset", {0, 0});
        offset = XYZVector(xy_offset.x(), xy_offset.y(), 0);
      }

      auto material = support_config.get<std::string>("material", "g10");
      std::transform(material.begin(), material.end(), material.begin(),
                     ::tolower);
      auto hole_size = support_config.get<XYVector>("hole_size", {0, 0});
      auto hole_offset = support_config.get<XYVector>("hole_offset", {0, 0});
      addSupportLayer(size, thickness, offset, material, location, hole_size,
                      hole_offset);
    }
  }
  /**
   * @brief Essential virtual destructor
   */
  virtual ~DetectorModel() = default;

  ///@{
  /**
   * @brief Use default copy and move behaviour
   */
  DetectorModel(const DetectorModel &) = default;
  DetectorModel &operator=(const DetectorModel &) = default;

  DetectorModel(DetectorModel &&) = default;
  DetectorModel &operator=(DetectorModel &&) = default;
  ///@}

  /**
   * @brief Get the configuration associated with this model
   * @return Configuration used to construct the model
   */
  std::vector<Configuration> getConfigurations() const {
    std::vector<Configuration> configurations;
    // Initialize global base configuration
    auto global_config_ = reader_.getHeaderConfiguration();

    for (auto &config : reader_.getConfigurations()) {
      if (config.getName().empty()) {
        // Merge all global sections with the global config
        global_config_.merge(config);
      } else {
        // Store all others
        configurations.push_back(config);
      }
    }

    // Prepend global config and return vector:
    configurations.insert(configurations.begin(), global_config_);
    return configurations;
  }

  /**
   * @brief Get the type of the model
   * @return Model type
   */
  std::string getType() const { return type_; }

  /**
   * @brief Get local coordinate of the position and rotation center in global
   * frame
   * @note It can be a counter intuitive that this is not usually the origin,
   * neither the geometric center of the model
   *
   * The center coordinate corresponds to the \ref Detector::getPosition
   * "position" in the global frame.
   */
  virtual ROOT::Math::XYZPoint getCenter() const {
    return ROOT::Math::XYZPoint(
        getGridSize().x() / 2.0 - getPixelSize().x() / 2.0,
        getGridSize().y() / 2.0 - getPixelSize().y() / 2.0, 0);
  }

  /**
   * @brief Get size of the box around the model that contains all elements
   * @return Size of the detector model
   *
   * All elements should be covered by a box with \ref DetectorModel::getCenter
   * as center. This means that the size
   * returned by this method is likely larger than the minimum possible size of
   * a box around all elements. It will only
   * return the minimum size if \ref DetectorModel::getCenter corresponds to the
   * geometric center of the model.
   */
  virtual ROOT::Math::XYZVector getSize() const {
    ROOT::Math::XYZVector max(std::numeric_limits<double>::lowest(),
                              std::numeric_limits<double>::lowest(),
                              std::numeric_limits<double>::lowest());
    ROOT::Math::XYZVector min(std::numeric_limits<double>::max(),
                              std::numeric_limits<double>::max(),
                              std::numeric_limits<double>::max());

    std::array<ROOT::Math::XYZPoint, 2> centers = {
        {getSensorCenter(), getChipCenter()}};
    std::array<ROOT::Math::XYZVector, 2> sizes = {
        {getSensorSize(), getChipSize()}};

    for (size_t i = 0; i < 2; ++i) {
      max.SetX(std::max(max.x(), (centers.at(i) + sizes.at(i) / 2.0).x()));
      max.SetY(std::max(max.y(), (centers.at(i) + sizes.at(i) / 2.0).y()));
      max.SetZ(std::max(max.z(), (centers.at(i) + sizes.at(i) / 2.0).z()));
      min.SetX(std::min(min.x(), (centers.at(i) - sizes.at(i) / 2.0).x()));
      min.SetY(std::min(min.y(), (centers.at(i) - sizes.at(i) / 2.0).y()));
      min.SetZ(std::min(min.z(), (centers.at(i) - sizes.at(i) / 2.0).z()));
    }

    for (auto &support_layer : getSupportLayers()) {
      auto size = support_layer.getSize();
      auto center = support_layer.getCenter();
      max.SetX(std::max(max.x(), (center + size / 2.0).x()));
      max.SetY(std::max(max.y(), (center + size / 2.0).y()));
      max.SetZ(std::max(max.z(), (center + size / 2.0).z()));
      min.SetX(std::min(min.x(), (center - size / 2.0).x()));
      min.SetY(std::min(min.y(), (center - size / 2.0).y()));
      min.SetZ(std::min(min.z(), (center - size / 2.0).z()));
    }

    ROOT::Math::XYZVector size;
    size.SetX(2 *
              std::max(max.x() - getCenter().x(), getCenter().x() - min.x()));
    size.SetY(2 *
              std::max(max.y() - getCenter().y(), getCenter().y() - min.y()));
    size.SetZ(2 *
              std::max(max.z() - getCenter().z(), getCenter().z() - min.z()));
    return size;
  }

  /* PIXEL GRID */
  /**
   * @brief Get number of pixel (replicated blocks in generic sensors)
   * @return Number of two dimensional pixels
   */
  virtual ROOT::Math::DisplacementVector2D<ROOT::Math::Cartesian2D<int>>
  getNPixels() const {
    return number_of_pixels_;
  }
  /**
   * @brief Set number of pixels (replicated blocks in generic sensors)
   * @param val Number of two dimensional pixels
   */
  void setNPixels(
      ROOT::Math::DisplacementVector2D<ROOT::Math::Cartesian2D<int>> val) {
    number_of_pixels_ = std::move(val);
  }
  /**
   * @brief Get size of a single pixel
   * @return Size of a pixel
   */
  virtual ROOT::Math::XYVector getPixelSize() const { return pixel_size_; }
  /**
   * @brief Set the size of a pixel
   * @param val Size of a pixel
   */
  void setPixelSize(ROOT::Math::XYVector val) { pixel_size_ = std::move(val); }
  /**
   * @brief Get total size of the pixel grid
   * @return Size of the pixel grid
   *
   * @warning The grid has zero thickness
   * @note This is basically a 2D method, but provided in 3D because it is
   * primarily used there
   */
  ROOT::Math::XYZVector getGridSize() const {
    return ROOT::Math::XYZVector(getNPixels().x() * getPixelSize().x(),
                                 getNPixels().y() * getPixelSize().y(), 0);
  }

  /* SENSOR */
  /**
   * @brief Get size of the sensor
   * @return Size of the sensor
   *
   * Calculated from \ref DetectorModel::getGridSize "pixel grid size", sensor
   * excess and sensor thickness
   */
  virtual ROOT::Math::XYZVector getSensorSize() const {
    ROOT::Math::XYZVector excess_thickness(
        (sensor_excess_.at(1) + sensor_excess_.at(3)),
        (sensor_excess_.at(0) + sensor_excess_.at(2)), sensor_thickness_);
    return getGridSize() + excess_thickness;
  }
  /**
   * @brief Get center of the sensor in local coordinates
   * @return Center of the sensor
   *
   * Center of the sensor with excess taken into account
   */
  virtual ROOT::Math::XYZPoint getSensorCenter() const {
    ROOT::Math::XYZVector offset(
        (sensor_excess_.at(1) - sensor_excess_.at(3)) / 2.0,
        (sensor_excess_.at(0) - sensor_excess_.at(2)) / 2.0, 0);
    return getCenter() + offset;
  }
  /**
   * @brief Set the thickness of the sensor
   * @param val Thickness of the sensor
   */
  void setSensorThickness(double val) { sensor_thickness_ = val; }
  /**
   * @brief Set the excess at the top of the sensor (positive y-coordinate)
   * @param val Sensor top excess
   */
  void setSensorExcessTop(double val) { sensor_excess_.at(0) = val; }
  /**
   * @brief Set the excess at the right of the sensor (positive x-coordinate)
   * @param val Sensor right excess
   */
  void setSensorExcessRight(double val) { sensor_excess_.at(1) = val; }
  /**
   * @brief Set the excess at the bottom of the sensor (negative y-coordinate)
   * @param val Sensor bottom excess
   */
  void setSensorExcessBottom(double val) { sensor_excess_.at(2) = val; }
  /**
   * @brief Set the excess at the left of the sensor (negative x-coordinate)
   * @param val Sensor right excess
   */
  void setSensorExcessLeft(double val) { sensor_excess_.at(3) = val; }

  /* CHIP */
  /**
   * @brief Get size of the chip
   * @return Size of the chip
   *
   * Calculated from \ref DetectorModel::getGridSize "pixel grid size", sensor
   * excess and chip thickness
   */
  virtual ROOT::Math::XYZVector getChipSize() const {
    ROOT::Math::XYZVector excess_thickness(
        (sensor_excess_.at(1) + sensor_excess_.at(3)),
        (sensor_excess_.at(0) + sensor_excess_.at(2)), chip_thickness_);
    return getGridSize() + excess_thickness;
  }
  /**
   * @brief Get center of the chip in local coordinates
   * @return Center of the chip
   *
   * Center of the chip calculcated from chip excess and sensor offset
   */
  virtual ROOT::Math::XYZPoint getChipCenter() const {
    ROOT::Math::XYZVector offset(
        (sensor_excess_.at(1) - sensor_excess_.at(3)) / 2.0,
        (sensor_excess_.at(0) - sensor_excess_.at(2)) / 2.0,
        getSensorSize().z() / 2.0 + getChipSize().z() / 2.0);
    return getCenter() + offset;
  }
  /**
   * @brief Set the thickness of the sensor
   * @param val Thickness of the sensor
   */
  void setChipThickness(double val) { chip_thickness_ = val; }

  /* SUPPORT */
  /**
   * @brief Return all layers of support
   * @return List of all the support layers
   *
   * This method internally computes the correct center of all the supports by
   * stacking them in linear order on both
   * the chip and the sensor side.
   */
  virtual std::vector<SupportLayer> getSupportLayers() const {
    auto ret_layers = support_layers_;

    auto sensor_offset = -getSensorSize().z() / 2.0;
    auto chip_offset = getSensorSize().z() / 2.0 + getChipSize().z();
    for (auto &layer : ret_layers) {
      ROOT::Math::XYZVector offset = layer.offset_;
      if (layer.location_ == "sensor") {
        offset.SetZ(sensor_offset - layer.size_.z() / 2.0);
        sensor_offset -= layer.size_.z();
      } else if (layer.location_ == "chip") {
        offset.SetZ(chip_offset + layer.size_.z() / 2.0);
        chip_offset += layer.size_.z();
      }

      layer.center_ = getCenter() + offset;
    }

    return ret_layers;
  }

  /**
   * @brief Add a new layer of support
   * @param size Size of the support in the x,y-plane
   * @param thickness Thickness of the support
   * @param offset Offset of the support in the x,y-plane
   * @param material Material of the support
   * @param location Location of the support (either 'sensor' or 'chip')
   * @param hole_size Size of the optional hole in the support
   * @param hole_offset Offset of the hole from its default position
   */
  // FIXME: Location (and material) should probably be an enum instead
  void addSupportLayer(const ROOT::Math::XYVector &size, double thickness,
                       ROOT::Math::XYZVector offset, std::string material,
                       std::string location,
                       const ROOT::Math::XYVector &hole_size,
                       ROOT::Math::XYVector hole_offset) {
    ROOT::Math::XYZVector full_size(size.x(), size.y(), thickness);
    ROOT::Math::XYZVector full_hole_size(hole_size.x(), hole_size.y(),
                                         thickness);
    support_layers_.push_back(SupportLayer(
        full_size, std::move(offset), std::move(material), std::move(location),
        full_hole_size, std::move(hole_offset)));
  }

protected:
  std::string type_;

  ROOT::Math::DisplacementVector2D<ROOT::Math::Cartesian2D<int>>
      number_of_pixels_;
  ROOT::Math::XYVector pixel_size_;

  double sensor_thickness_{};
  std::array<double, 4> sensor_excess_{};

  double chip_thickness_{};

  std::vector<SupportLayer> support_layers_;

private:
  ConfigReader reader_;
};
} // namespace allpix

#endif // ALLPIX_DETECTOR_MODEL_H
