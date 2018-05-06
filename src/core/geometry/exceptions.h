/**
 * @file
 * @brief Collection of all geometry exceptions
 *
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied
 * verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities
 * granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#ifndef ALLPIX_GEOMETRY_EXCEPTIONS_H
#define ALLPIX_GEOMETRY_EXCEPTIONS_H

#include <string>

#include "core/utils/exceptions.h"
#include "core/utils/type.h"

namespace allpix {
/**
 * @ingroup Exceptions
 * @brief Indicates an error with finding a detector by name
 */
class InvalidDetectorError : public RuntimeError {
public:
  /**
   * @brief Constructs an error with a detector that is not found
   * @param name Identifier for the detector that is not found
   */
  explicit InvalidDetectorError(const std::string &name) {
    error_message_ = "Could not find a detector with name '" + name + "'";
  }
};

/**
 * @ingroup Exceptions
 * @brief Indicates an error that the detector model is not found
 */
class InvalidModelError : public RuntimeError {
public:
  /**
   * @brief Constructs an error with a model that is not found
   * @param name Identifier for the model that is not found
   */
  explicit InvalidModelError(const std::string &name) {
    error_message_ = "Could not find a detector model of type '" + name + "'";
  }
};

/**
 * @ingroup Exceptions
 * @brief Indicates an attempt to add a detector that is already registered
 * before
 */
class DetectorExistsError : public RuntimeError {
public:
  /**
   * @brief Constructs an error for a non unique detector
   * @param name Name of the detector that is added earlier
   */
  explicit DetectorExistsError(const std::string &name) {
    error_message_ = "Detector with name " + name +
                     " is already registered, detector names should be unique";
  }
};

/**
 * @ingroup Exceptions
 * @brief Indicates an attempt to add a detector model that is already
 * registered before
 */
class DetectorModelExistsError : public RuntimeError {
public:
  /**
   * @brief Constructs an error for a non unique model
   * @param name Name of the model that is added earlier
   */
  explicit DetectorModelExistsError(const std::string &name) {
    error_message_ = "Model with type " + name +
                     " is already registered, detector names should be unique";
  }
};

/**
 * @ingroup Exceptions
 * @brief Indicates an attempt to add a detector with an invalid name
 */
class DetectorInvalidNameError : public RuntimeError {
public:
  /**
   * @brief Constructs an error for a detector with an invalid name
   * @param name Invalid name of the detector that is attempted to be added
   */
  explicit DetectorInvalidNameError(const std::string &name) {
    error_message_ =
        "Detector name " + name + " is invalid, choose a different name";
  }
};
} // namespace allpix
#endif /* ALLPIX_GEOMETRY_EXCEPTIONS_H */
