/**
 * @file
 * @brief Core object of the configuration system
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied
 * verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities
 * granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#ifndef ALLPIX_CONFIGURATION_H
#define ALLPIX_CONFIGURATION_H

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/utils/text.h"
#include "exceptions.h"

namespace allpix {

template <typename T> using Matrix = std::vector<std::vector<T>>;

/**
 * @brief Generic configuration object storing keys
 *
 * The configuration holds a set of keys with arbitrary values that are
 * internally stored as strings. It has special
 * logic for reading paths (relative to the configuration file). All types are
 * converted to their appropriate type using
 * the library of \ref StringConversions.
 */
class Configuration {
public:
  /**
   * @brief Construct a configuration object
   * @param name Name of the section header (empty section if not specified)
   * @param path Path to the file containing the configuration (or empty if not
   * stored in a file)
   */
  explicit Configuration(std::string name = "", std::string path = "");

  /// @{
  /**
   * @brief Allow copying the configuration
   */
  Configuration(const Configuration &) = default;
  Configuration &operator=(const Configuration &) = default;
  /// @}

  /// @{
  /**
   * @brief Allow moving the configuration
   */
  Configuration(Configuration &&) noexcept = default;
  Configuration &operator=(Configuration &&) noexcept = default;
  /// @}

  /**
   * @brief Check if key is defined
   * @param key Key to check for existence
   * @return True if key exists, false otherwise
   */
  bool has(const std::string &key) const;

  /**
   * @brief Check how many of the given keys are defined
   * @param keys Keys to check for existence
   * @return number of existing keys from the given list
   */
  unsigned int count(std::initializer_list<std::string> keys) const;

  /**
   * @brief Get value of a key in requested type
   * @param key Key to get value of
   * @return Value of the key in the type of the requested template parameter
   */
  template <typename T> T get(const std::string &key) const;
  /**
   * @brief Get value of a key in requested type or default value if it does not
   * exists
   * @param key Key to get value of
   * @param def Default value to use if key is not defined
   * @return Value of the key in the type of the requested template parameter
   *         or the default value if the key does not exists
   */
  template <typename T> T get(const std::string &key, const T &def) const;

  /**
   * @brief Get values for a key containing an array
   * @param key Key to get values of
   * @return List of values in the array in the requested template parameter
   */
  // TODO [doc] Provide second template parameter to specify the vector type to
  // return it in
  template <typename T> std::vector<T> getArray(const std::string &key) const;

  /**
   * @brief Get values for a key containing an array or default array if it does
   * not exists
   * @param key Key to get values of
   * @param def Default value array to use if key is not defined
   * @return List of values in the array in the requested template parameter
   *         or the default array if the key does not exist
   */
  // TODO [doc] Provide second template parameter to specify the vector type to
  // return it in
  template <typename T>
  std::vector<T> getArray(const std::string &key,
                          const std::vector<T> def) const;

  /**
   * @brief Get values for a key containing a 2D matrix
   * @param key Key to get values of
   * @return Matrix of values from the requested template parameter
   */
  template <typename T> Matrix<T> getMatrix(const std::string &key) const;

  /**
   * @brief Get values for a key containing a 2D matrix
   * @param key Key to get values of
   * @param def Default value matrix to use if key is not defined
   * @return Matrix of values from the requested template parameter
   */
  template <typename T>
  Matrix<T> getMatrix(const std::string &key, const Matrix<T> def) const;

  /**
   * @brief Get literal value of a key as string
   * @param key Key to get values of
   * @return Literal value of the key
   * @note This function does also not remove quotation marks in strings
   */
  std::string getText(const std::string &key) const;
  /**
   * @brief Get literal value of a key as string or a default if it does not
   * exists
   * @param key Key to get values of
   * @param def Default value to use if key is not defined
   * @return Literal value of the key or the default value if the key does not
   * exists
   * @note This function does also not remove quotation marks in strings
   */
  std::string getText(const std::string &key, const std::string &def) const;

  /**
   * @brief Get absolute path to file with paths relative to the configuration
   * @param key Key to get path of
   * @param check_exists If the file should be checked for existence (if yes
   * always returns a canonical path)
   * @return Absolute path to a file
   */
  std::string getPath(const std::string &key, bool check_exists = false) const;
  /**
   * @brief Get array of absolute paths to files with paths relative to the
   * configuration
   * @param key Key to get path of
   * @param check_exists If the files should be checked for existence (if yes
   * always returns a canonical path)
   * @return List of absolute path to all the requested files
   */
  // TODO [doc] Provide second template parameter to specify the vector type to
  // return it in
  std::vector<std::string> getPathArray(const std::string &key,
                                        bool check_exists = false) const;

  /**
   * @brief Set value for a key in a given type
   * @param key Key to set value of
   * @param val Value to assign to the key
   */
  template <typename T> void set(const std::string &key, const T &val);
  /**
   * @brief Set list of values for a key in a given type
   * @param key Key to set values of
   * @param val List of values to assign to the key
   */
  // TODO [doc] Provide second template parameter to specify the vector type to
  // return it in
  template <typename T>
  void setArray(const std::string &key, const std::vector<T> &val);

  /**
   * @brief Set default value for a key only if it is not defined yet
   * @param key Key to possible set value of
   * @param val Value to assign if the key is not defined yet
   */
  template <typename T> void setDefault(const std::string &key, const T &val);
  /**
   * @brief Set default list of values for a key only if it is not defined yet
   * @param key Key to possible set values of
   * @param val List of values to assign to the key if the key is not defined
   * yet
   */
  template <typename T>
  void setDefaultArray(const std::string &key, const std::vector<T> &val);

  /**
   * @brief Set literal value of a key as string
   * @param key Key to set value of
   * @param val Literal string to assign key to
   */
  void setText(const std::string &key, const std::string &val);

  /**
   * @brief Set alias name for an already existing key
   * @param new_key New alias to be created
   * @param old_key Key the alias is created for
   */
  void setAlias(const std::string &new_key, const std::string &old_key);

  /**
   * @brief Return total number of key / value pairs
   * @return Number of settings
   */
  unsigned int countSettings() const;

  /**
   * @brief Get name of the configuration header
   * @return Configuration name
   */
  std::string getName() const;

  /**
   * @brief Get path to the file containing the configuration if it has one
   * @return Absolute path to configuration file or empty if not linked to a
   * file
   * @warning Parameter should be used with care as not all configurations are
   * required to have a file
   */
  // TODO [doc] Fix name clash with getPath
  std::string getFilePath() const;

  /**
   * @brief Merge other configuration, only adding keys that are not yet defined
   * in this configuration
   * @param other Configuration to merge this one with
   */
  void merge(const Configuration &other);

  /**
   * @brief Get all key value pairs
   * @return List of all key value pairs
   */
  // FIXME Better name for this function
  std::vector<std::pair<std::string, std::string>> getAll();

private:
  /**
   * @brief Make relative paths absolute from this configuration file
   * @param path Path to make absolute (if it is not already absolute)
   * @param canonicalize_path If the path should be canonicalized (throws an
   * error if the path does not exist)
   */
  std::string path_to_absolute(std::string path, bool canonicalize_path) const;

  /**
   * @brief Node in a parse tree
   */
  struct parse_node {
    std::string value;
    std::vector<std::unique_ptr<parse_node>> children;
  };
  /**
   * @brief Generate parse tree from configuration string
   * @param str String to parse
   * @param depth Current depth of the parsing (starts at zero)
   * @return Root node of the parsed tree
   */
  static std::unique_ptr<parse_node> parse_value(std::string str,
                                                 int depth = 0);

  std::string name_;
  std::string path_;

  using ConfigMap = std::map<std::string, std::string>;
  ConfigMap config_;
};
} // namespace allpix

// Include template members
#include "Configuration.tpp"

#endif /* ALLPIX_CONFIGURATION_H */
