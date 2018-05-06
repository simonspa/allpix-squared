/**
 * @file
 * @brief Implementation of geometry manager
 *
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied
 * verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities
 * granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <Math/RotationX.h>
#include <Math/RotationY.h>
#include <Math/RotationZ.h>
#include <Math/RotationZYX.h>
#include <Math/Vector3D.h>

#include "GeometryManager.hpp"
#include "core/config/ConfigReader.hpp"
#include "core/module/exceptions.h"
#include "core/utils/file.h"
#include "core/utils/log.h"
#include "core/utils/unit.h"
#include "exceptions.h"
#include "tools/ROOT.h"

#include "core/geometry/HybridPixelDetectorModel.hpp"
#include "core/geometry/MonolithicPixelDetectorModel.hpp"

using namespace allpix;
using namespace ROOT::Math;

GeometryManager::GeometryManager() : closed_{false} {}

/**
 * Loads the geometry by looping over all defined detectors
 */
void GeometryManager::load(ConfigManager *conf_manager,
                           std::mt19937_64 &seeder) {
  // Set up a random number generator and seed it with the global seed:
  std::mt19937_64 random_generator;
  random_generator.seed(seeder());

  // Loop over all defined detectors
  LOG(DEBUG) << "Loading detectors";
  for (auto &detector_section : conf_manager->getDetectorConfigurations()) {
    LOG(DEBUG) << "Detector " << detector_section.getName() << ":";

    // Calculate possible detector misalignment to be added
    auto misalignment = [&](auto residuals) {
      double dx =
          std::normal_distribution<double>(0, residuals.x())(random_generator);
      double dy =
          std::normal_distribution<double>(0, residuals.y())(random_generator);
      double dz =
          std::normal_distribution<double>(0, residuals.z())(random_generator);
      return DisplacementVector3D<Cartesian3D<double>>(dx, dy, dz);
    };

    // Get the position and apply potenial misalignment
    auto position = detector_section.get<XYZPoint>("position", XYZPoint());
    LOG(DEBUG) << "Position:    " << Units::display(position, {"mm", "um"});
    position += misalignment(detector_section.get<XYZPoint>(
        "alignment_precision_position", XYZPoint()));
    LOG(DEBUG) << " misaligned: " << Units::display(position, {"mm", "um"});

    // Get the orientation and apply misalignment to the individual angles
    // before combining them
    auto orient_vec =
        detector_section.get<XYZVector>("orientation", XYZVector());
    LOG(DEBUG) << "Orientation: " << Units::display(orient_vec, {"deg"});
    orient_vec += misalignment(detector_section.get<XYZVector>(
        "alignment_precision_orientation", XYZVector()));
    LOG(DEBUG) << " misaligned: " << Units::display(orient_vec, {"deg"});

    auto orientation_mode =
        detector_section.get<std::string>("orientation_mode", "xyz");
    Rotation3D orientation;

    if (orientation_mode == "zyx") {
      // First angle given in the configuration file is around z, second around
      // y, last around x:
      LOG(DEBUG) << "Interpreting Euler angles as ZYX rotation";
      orientation = RotationZYX(orient_vec.x(), orient_vec.y(), orient_vec.z());
    } else if (orientation_mode == "xyz") {
      LOG(DEBUG) << "Interpreting Euler angles as XYZ rotation";
      // First angle given in the configuration file is around x, second around
      // y, last around z:
      orientation = RotationZ(orient_vec.z()) * RotationY(orient_vec.y()) *
                    RotationX(orient_vec.x());
    } else if (orientation_mode == "zxz") {
      LOG(DEBUG) << "Interpreting Euler angles as ZXZ rotation";
      // First angle given in the configuration file is around z, second around
      // x, last around z:
      orientation = EulerAngles(orient_vec.x(), orient_vec.y(), orient_vec.z());
    } else {
      throw InvalidValueError(
          detector_section, "orientation_mode",
          "orientation_mode should be either 'zyx', xyz' or 'zxz'");
    }

    // Create the detector and add it without model
    // NOTE: cannot use make_shared here due to the private constructor
    auto detector = std::shared_ptr<Detector>(
        new Detector(detector_section.getName(), position, orientation));
    addDetector(detector);

    // Add a link to the detector to add the model later
    nonresolved_models_[detector_section.get<std::string>("type")].emplace_back(
        detector_section, detector.get());
  }

  // Load the list of standard model paths
  Configuration &global_config = conf_manager->getGlobalConfiguration();
  if (global_config.has("model_paths")) {
    auto extra_paths = global_config.getPathArray("model_paths", true);
    model_paths_.insert(model_paths_.end(), extra_paths.begin(),
                        extra_paths.end());
    LOG(TRACE) << "Registered model paths from configuration.";
  }
  if (path_is_directory(ALLPIX_MODEL_DIRECTORY)) {
    model_paths_.emplace_back(ALLPIX_MODEL_DIRECTORY);
    LOG(TRACE) << "Registered model path: " << ALLPIX_MODEL_DIRECTORY;
  }
  const char *data_dirs_env = std::getenv("XDG_DATA_DIRS");
  if (data_dirs_env == nullptr || strlen(data_dirs_env) == 0) {
    data_dirs_env = "/usr/local/share/:/usr/share/:";
  }
  std::vector<std::string> data_dirs = split<std::string>(data_dirs_env, ":");
  for (auto data_dir : data_dirs) {
    if (data_dir.back() != '/') {
      data_dir += "/";
    }

    data_dir += std::string(ALLPIX_PROJECT_NAME) + std::string("/models");
    if (path_is_directory(data_dir)) {
      model_paths_.emplace_back(data_dir);
      LOG(TRACE) << "Registered global model path: " << data_dir;
    }
  }
}

/**
 * The default list of models to search for are in the following order
 * - The list of paths provided in the main configuration as model_paths
 * - The build variable ALLPIX_MODEL_DIR pointing to the installation directory
 * of the framework models
 * - The directories in XDG_DATA_DIRS with ALLPIX_PROJECT_NAME attached or
 * /usr/share/:/usr/local/share if not defined
 */
std::vector<std::string> GeometryManager::getModelsPath() {
  return model_paths_;
}

/**
 * The minimum coordinate is the location of the point where no part of any
 * detector exist with a lower x, y or z-coordinate
 * in the geometry. The minimum point is never above the origin (the origin is
 * always included in the geometry).
 */
ROOT::Math::XYZPoint GeometryManager::getMinimumCoordinate() {
  if (!closed_) {
    close_geometry();
  }

  ROOT::Math::XYZPoint min_point(0, 0, 0);
  // Loop through all detector
  for (auto &detector : detectors_) {
    // Get the model of the detector
    auto model = detector->getModel();

    std::array<int, 8> offset_x = {{1, 1, 1, 1, -1, -1, -1, -1}};
    std::array<int, 8> offset_y = {{1, 1, -1, -1, 1, 1, -1, -1}};
    std::array<int, 8> offset_z = {{1, -1, 1, -1, 1, -1, 1, -1}};

    for (size_t i = 0; i < 8; ++i) {
      auto point = model->getCenter();
      point.SetX(point.x() + offset_x.at(i) * model->getSize().x() / 2.0);
      point.SetY(point.y() + offset_y.at(i) * model->getSize().y() / 2.0);
      point.SetZ(point.z() + offset_z.at(i) * model->getSize().z() / 2.0);
      point = detector->getGlobalPosition(point);

      min_point.SetX(std::min(min_point.x(), point.x()));
      min_point.SetY(std::min(min_point.y(), point.y()));
      min_point.SetZ(std::min(min_point.z(), point.z()));
    }
  }

  // Loop through all separate points
  for (auto &point : points_) {
    min_point.SetX(std::min(min_point.x(), point.x()));
    min_point.SetY(std::min(min_point.y(), point.y()));
    min_point.SetZ(std::min(min_point.z(), point.z()));
  }

  return min_point;
}

/**
 * The maximum coordinate is the location of the point where no part of any
 * detector exist with a higher x, y or z-coordinate
 * in the geometry. The maximum point is never below the origin (the origin is
 * always included in the geometry).
 */
ROOT::Math::XYZPoint GeometryManager::getMaximumCoordinate() {
  if (!closed_) {
    close_geometry();
  }

  ROOT::Math::XYZPoint max_point(0, 0, 0);
  // Loop through all detector
  for (auto &detector : detectors_) {
    // Get the model of the detector
    auto model = detector->getModel();

    std::array<int, 8> offset_x = {{1, 1, 1, 1, -1, -1, -1, -1}};
    std::array<int, 8> offset_y = {{1, 1, -1, -1, 1, 1, -1, -1}};
    std::array<int, 8> offset_z = {{1, -1, 1, -1, 1, -1, 1, -1}};

    for (size_t i = 0; i < 8; ++i) {
      auto point = model->getCenter();
      point.SetX(point.x() + offset_x.at(i) * model->getSize().x() / 2.0);
      point.SetY(point.y() + offset_y.at(i) * model->getSize().y() / 2.0);
      point.SetZ(point.z() + offset_z.at(i) * model->getSize().z() / 2.0);
      point = detector->getGlobalPosition(point);

      max_point.SetX(std::max(max_point.x(), point.x()));
      max_point.SetY(std::max(max_point.y(), point.y()));
      max_point.SetZ(std::max(max_point.z(), point.z()));
    }
  }

  // Loop through all separate points
  for (auto &point : points_) {
    max_point.SetX(std::max(max_point.x(), point.x()));
    max_point.SetY(std::max(max_point.y(), point.y()));
    max_point.SetZ(std::max(max_point.z(), point.z()));
  }

  return max_point;
}

/**
 * @throws ModuleError If the geometry is already closed before calling this
 * function
 *
 * Can be used to add an arbitrary and unspecified point which is part of the
 * geometry
 */
void GeometryManager::addPoint(ROOT::Math::XYZPoint point) {
  if (closed_) {
    throw ModuleError("Geometry is already closed before adding detector");
  }
  points_.push_back(std::move(point));
}

/**
 * @throws InvalidModuleActionException If the passed detector is a null pointer
 * @throws ModuleError If the geometry is already closed before calling this
 * function
 * @throws DetectorModelExistsError If the detector name is already registered
 * before
 */
void GeometryManager::addModel(std::shared_ptr<DetectorModel> model) {
  if (closed_) {
    throw ModuleError("Geometry is already closed before adding detector");
  }
  if (model == nullptr) {
    throw InvalidModuleActionException("Added model cannot be a null pointer");
  }

  LOG(TRACE) << "Registering new model " << model->getType();
  if (model_names_.find(model->getType()) != model_names_.end()) {
    throw DetectorModelExistsError(model->getType());
  }

  model_names_.insert(model->getType());
  models_.push_back(std::move(model));
}

bool GeometryManager::needsModel(const std::string &name) const {
  return nonresolved_models_.find(name) != nonresolved_models_.end();
}
bool GeometryManager::hasModel(const std::string &name) const {
  return model_names_.find(name) != model_names_.end();
}

std::vector<std::shared_ptr<DetectorModel>> GeometryManager::getModels() const {
  return models_;
}
/**
 * @throws InvalidDetectorError If a model with this name does not exist
 */
std::shared_ptr<DetectorModel>
GeometryManager::getModel(const std::string &name) const {
  // FIXME: this is not a very nice way to implement this
  for (auto &model : models_) {
    if (model->getType() == name) {
      return model;
    }
  }
  throw allpix::InvalidModelError(name);
}

/**
 * @throws InvalidModuleActionException If the passed detector is a null pointer
 * @throws ModuleError If the geometry is already closed before calling this
 * function
 * @throws DetectorInvalidNameError If the detector name is invalid
 * @throws DetectorNameExistsError If the detector name is already registered
 * before
 */
void GeometryManager::addDetector(std::shared_ptr<Detector> detector) {
  if (closed_) {
    throw ModuleError("Geometry is already closed before adding detector");
  }
  if (detector == nullptr) {
    throw InvalidModuleActionException(
        "Added detector cannot be a null pointer");
  }

  LOG(TRACE) << "Registering new detector " << detector->getName();

  // The name global is used for objects not assigned to any detector, hence it
  // shouldn't be used as a detector name
  if (detector->getName() == "global") {
    throw DetectorInvalidNameError(detector->getName());
  }

  if (detector_names_.find(detector->getName()) != detector_names_.end()) {
    throw DetectorExistsError(detector->getName());
  }

  detector_names_.insert(detector->getName());
  detectors_.push_back(std::move(detector));
}

bool GeometryManager::hasDetector(const std::string &name) const {
  return detector_names_.find(name) != detector_names_.end();
}

std::vector<std::shared_ptr<Detector>> GeometryManager::getDetectors() {
  if (!closed_) {
    close_geometry();
  }

  return detectors_;
}
/**
 * @throws InvalidDetectorError If a detector with this name does not exist
 */
std::shared_ptr<Detector>
GeometryManager::getDetector(const std::string &name) {
  if (!closed_) {
    close_geometry();
  }

  // FIXME: this is not a very nice way to implement this
  for (auto &detector : detectors_) {
    if (detector->getName() == name) {
      return detector;
    }
  }
  throw allpix::InvalidDetectorError(name);
}
/**
 * @throws InvalidDetectorError If not a single detector with this type exists
 */
std::vector<std::shared_ptr<Detector>>
GeometryManager::getDetectorsByType(const std::string &type) {
  if (!closed_) {
    close_geometry();
  }

  std::vector<std::shared_ptr<Detector>> result;
  for (auto &detector : detectors_) {
    if (detector->getType() == type) {
      result.push_back(detector);
    }
  }
  if (result.empty()) {
    throw allpix::InvalidModelError(type);
  }

  return result;
}

void GeometryManager::load_models() {
  LOG(TRACE) << "Loading remaining default models";

  // Get paths to read models from
  std::vector<std::string> paths = getModelsPath();

  std::vector<std::pair<std::string, ConfigReader>> readers;
  LOG(TRACE) << "Reading model files";
  // Add all the paths to the reader
  for (auto &path : paths) {
    // Check if file or directory
    if (allpix::path_is_directory(path)) {
      std::vector<std::string> sub_paths = allpix::get_files_in_directory(path);
      for (auto &sub_path : sub_paths) {
        auto name_ext = allpix::get_file_name_extension(sub_path);

        // Accept only with correct model suffix
        std::string suffix(ALLPIX_MODEL_SUFFIX);
        if (name_ext.second != suffix) {
          continue;
        }

        // Add the sub directory path to the reader
        LOG(TRACE) << "Reading model " << sub_path;
        std::ifstream file(sub_path);

        ConfigReader reader(file, sub_path);
        readers.emplace_back(name_ext.first, reader);
      }
    } else {
      // Always a file because paths are already checked
      LOG(TRACE) << "Reading model " << path;
      std::ifstream file(path);

      ConfigReader reader(file, path);
      auto name_ext = allpix::get_file_name_extension(path);
      readers.emplace_back(name_ext.first, reader);
    }
  }

  // Loop through all configurations and parse them
  LOG(TRACE) << "Parsing models";
  for (auto &name_reader : readers) {
    if (hasModel(name_reader.first)) {
      // Skip models that we already loaded earlier higher in the chain
      LOG(DEBUG) << "Skipping overwritten model " + name_reader.first
                 << " in path "
                 << name_reader.second.getHeaderConfiguration().getFilePath();
      continue;
    }
    if (!needsModel(name_reader.first)) {
      // Also skip models that are not needed
      LOG(TRACE) << "Skipping not required model " + name_reader.first
                 << " in path "
                 << name_reader.second.getHeaderConfiguration().getFilePath();
      continue;
    }

    // Parse configuration and add model to the config
    addModel(parse_config(name_reader.first, name_reader.second));
  }
}

std::shared_ptr<DetectorModel>
GeometryManager::parse_config(const std::string &name,
                              const ConfigReader &reader) {
  Configuration config = reader.getHeaderConfiguration();

  if (!config.has("type")) {
    LOG(ERROR) << "Model file " << config.getFilePath()
               << " does not provide a type parameter";
  }
  auto type = config.get<std::string>("type");

  // Instantiate the correct detector model
  if (type == "hybrid") {
    return std::make_shared<HybridPixelDetectorModel>(name, reader);
  }
  if (type == "monolithic") {
    return std::make_shared<MonolithicPixelDetectorModel>(name, reader);
  }

  LOG(ERROR) << "Model file " << config.getFilePath()
             << " type parameter is not valid";
  // FIXME: The model can probably be silently ignored if we have more model
  // readers later
  throw InvalidValueError(config, "type", "model type is not supported");
}

/*
 * After closing the geometry new parts of the geometry cannot be added anymore.
 * All the models for the detectors in the
 * configuration are resolved to requested type (and an error is thrown if this
 * is not possible). Also if a parameter is
 * specialized in the detector config a copy of the model is created with those
 * specialized settings.
 */
void GeometryManager::close_geometry() {
  LOG(TRACE) << "Starting geometry closing procedure";

  // Load all standard models
  load_models();

  // Try to resolve the missing models
  for (auto &detectors_types : nonresolved_models_) {
    for (auto &config_detector : detectors_types.second) {
      // Create a new model if one of the core model parameters is changed in
      // the detector configuration
      auto config = config_detector.first;
      auto model = getModel(detectors_types.first);

      // Get the configuration of the model
      Configuration new_config("");
      auto model_configs = model->getConfigurations();

      // Add all non internal parameters to the config for a specialized model
      for (auto &key_value : config.getAll()) {
        auto key = key_value.first;
        // Skip all internal parameters
        if (key == "type" || key == "position" || key == "orientation_mode" ||
            key == "orientation") {
          continue;
        }
        // Add the extra parameter to the new overwritten config
        new_config.setText(key, key_value.second);
      }

      // Create new model if needed
      if (new_config.countSettings() != 0) {
        ConfigReader reader;
        // Add the new configuration first to overwrite
        reader.addConfiguration(std::move(new_config));
        // Then add the original configuration
        for (auto &model_config : model_configs) {
          reader.addConfiguration(std::move(model_config));
        }

        model = parse_config(detectors_types.first, reader);
      }

      config_detector.second->set_model(model);
    }
  }

  closed_ = true;
  LOG(TRACE) << "Closed geometry";
}

bool GeometryManager::hasMagneticField() const {
  return (magnetic_field_type_ != MagneticFieldType::NONE);
}

void GeometryManager::setMagneticFieldFunction(MagneticFieldFunction function,
                                               MagneticFieldType type) {
  magnetic_field_function_ = std::move(function);
  magnetic_field_type_ = type;
}

MagneticFieldType GeometryManager::getMagneticFieldType() const {
  return magnetic_field_type_;
}

ROOT::Math::XYZVector
GeometryManager::getMagneticField(const ROOT::Math::XYZPoint &position) const {
  return magnetic_field_function_(position);
}
