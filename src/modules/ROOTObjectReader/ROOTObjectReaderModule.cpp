/**
 * @file
 * @brief Implementation of ROOT data file reader module
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied
 * verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities
 * granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#include "ROOTObjectReaderModule.hpp"

#include <climits>
#include <string>
#include <utility>

#include <TBranch.h>
#include <TKey.h>
#include <TObjArray.h>
#include <TTree.h>

#include "core/messenger/Messenger.hpp"
#include "core/utils/log.h"
#include "core/utils/type.h"

#include "objects/Object.hpp"
#include "objects/objects.h"

#include "core/utils/type.h"

using namespace allpix;

ROOTObjectReaderModule::ROOTObjectReaderModule(Configuration &config,
                                               Messenger *messenger,
                                               GeometryManager *geo_mgr)
    : Module(config), messenger_(messenger), geo_mgr_(geo_mgr) {}

/**
 * @note Objects cannot be stored in smart pointers due to internal ROOT logic
 */
ROOTObjectReaderModule::~ROOTObjectReaderModule() {
  for (auto message_inf : message_info_array_) {
    delete message_inf.objects;
  }
}

/**
 * Adds lambda function map to convert a vector of generic objects to a
 * templated message containing this particular type of
 * object from its typeid.
 */
template <typename T>
static void add_creator(ROOTObjectReaderModule::MessageCreatorMap &map) {
  map[typeid(T)] = [&](std::vector<Object *> objects,
                       std::shared_ptr<Detector> detector = nullptr) {
    std::vector<T> data;
    for (auto &object : objects) {
      data.emplace_back(std::move(*static_cast<T *>(object)));
    }

    if (detector == nullptr) {
      return std::make_shared<Message<T>>(data);
    }
    return std::make_shared<Message<T>>(data, detector);
  };
}

/**
 * Uses SFINAE trick to call the add_creator function for all template arguments
 * of a container class. Used to add creators
 * for every object in a tuple of objects.
 */
template <template <typename...> class T, typename... Args>
static void
gen_creator_map_from_tag(ROOTObjectReaderModule::MessageCreatorMap &map,
                         type_tag<T<Args...>>) {
  std::initializer_list<int> value{(add_creator<Args>(map), 0)...};
  (void)value;
}

/**
 * Wrapper function to make the SFINAE trick in \ref gen_creator_map_from_tag
 * work.
 */
template <typename T>
static ROOTObjectReaderModule::MessageCreatorMap gen_creator_map() {
  ROOTObjectReaderModule::MessageCreatorMap ret_map;
  gen_creator_map_from_tag(ret_map, type_tag<T>());
  return ret_map;
}

void ROOTObjectReaderModule::init() {
  // Read include and exclude list
  if (config_.has("include") && config_.has("exclude")) {
    throw InvalidCombinationError(
        config_, {"exclude", "include"},
        "include and exclude parameter are mutually exclusive");
  } else if (config_.has("include")) {
    auto inc_arr = config_.getArray<std::string>("include");
    include_.insert(inc_arr.begin(), inc_arr.end());
  } else if (config_.has("exclude")) {
    auto exc_arr = config_.getArray<std::string>("exclude");
    exclude_.insert(exc_arr.begin(), exc_arr.end());
  }

  // Initialize the call map from the tuple of available objects
  message_creator_map_ = gen_creator_map<allpix::OBJECTS>();

  // Open the file with the objects
  input_file_ =
      std::make_unique<TFile>(config_.getPath("file_name", true).c_str());

  // Read all the trees in the file
  TList *keys = input_file_->GetListOfKeys();
  std::set<std::string> tree_names;

  for (auto &&object : *keys) {
    auto &key = dynamic_cast<TKey &>(*object);
    if (std::string(key.GetClassName()) == "TTree") {
      auto tree = static_cast<TTree *>(key.ReadObjectAny(nullptr));

      // Check if a version of this tree has already been read
      if (tree_names.find(tree->GetName()) != tree_names.end()) {
        LOG(TRACE)
            << "Skipping copy of tree with name " << tree->GetName()
            << " because one with identical name has already been processed";
        continue;
      }
      tree_names.insert(tree->GetName());

      // Check if this tree should be used
      if ((!include_.empty() &&
           include_.find(tree->GetName()) == include_.end()) ||
          (!exclude_.empty() &&
           exclude_.find(tree->GetName()) != exclude_.end())) {
        LOG(TRACE) << "Ignoring tree with " << tree->GetName()
                   << " objects because it has been excluded or not explicitly "
                      "included";
        continue;
      }

      trees_.push_back(tree);
    }
  }

  if (trees_.empty()) {
    LOG(ERROR) << "Provided ROOT file does not contain any trees, module will "
                  "not read any data";
  }

  // Cross-check the core random seed stored in the file with the one
  // configured:
  auto &global_config = getConfigManager()->getGlobalConfiguration();
  auto config_seed = global_config.get<uint64_t>("random_seed_core");

  std::string *str = nullptr;
  input_file_->GetObject("config/Allpix/random_seed_core", str);
  if (str == nullptr) {
    throw InvalidValueError(
        global_config, "random_seed_core",
        "no random seed for core set in the input data file, cross-check with "
        "configured value "
        "impossible - this might lead to unexpected behavior.");
  }

  auto file_seed = allpix::from_string<uint64_t>(*str);
  if (config_seed != file_seed) {
    throw InvalidValueError(global_config, "random_seed_core",
                            "mismatch between core random seed in "
                            "configuration file and input data - this "
                            "might lead to unexpected behavior. Set to value "
                            "configured in the input data file: " +
                                (*str));
  }

  // Cross-check version, print warning only in case of a mismatch:
  std::string *version_str = nullptr;
  input_file_->GetObject("config/Allpix/version", version_str);
  if (version_str != nullptr && (*version_str) != ALLPIX_PROJECT_VERSION) {
    LOG(WARNING) << "Reading data produced with different version "
                 << (*version_str)
                 << " - this might lead to unexpected behavior.";
  }

  // Loop over all found trees
  for (auto &tree : trees_) {
    // Loop over the list of branches and create the set of receiver objects
    TObjArray *branches = tree->GetListOfBranches();
    for (int i = 0; i < branches->GetEntries(); i++) {
      auto *branch = static_cast<TBranch *>(branches->At(i));

      // Add a new vector of objects and bind it to the branch
      message_info message_inf;
      message_inf.objects = new std::vector<Object *>;
      message_info_array_.emplace_back(message_inf);
      branch->SetAddress(&(message_info_array_.back().objects));

      // Fill the rest of the message information
      // FIXME: we want to index this in a different way
      std::string branch_name = branch->GetName();
      auto split = allpix::split<std::string>(branch_name, "_");

      // Fetch information from the tree name
      size_t expected_size = 2;
      size_t det_idx = 0;
      size_t name_idx = 1;
      if (branch_name.front() == '_' || branch_name.empty()) {
        --expected_size;
        det_idx = INT_MAX;
        --name_idx;
      }
      if (branch_name.find('_') == std::string::npos) {
        --expected_size;
        name_idx = INT_MAX;
      }

      // Check tree structure and if object type matches name
      auto split_type =
          allpix::split<std::string>(branch->GetClassName(), "<>");
      if (expected_size != split.size() || split_type.size() != 2 ||
          split_type[1].size() <= 2) {
        throw ModuleError(
            "Tree is malformed and cannot be used for creating messages");
      }
      std::string class_name =
          split_type[1].substr(0, split_type[1].size() - 1);
      std::string apx_namespace = "allpix::";
      size_t ap_idx = class_name.find(apx_namespace);
      if (ap_idx != std::string::npos) {
        class_name.replace(ap_idx, apx_namespace.size(), "");
      }
      if (class_name != tree->GetName()) {
        throw ModuleError("Tree contains objects of the wrong type");
      }

      if (name_idx != INT_MAX) {
        message_info_array_.back().name = split[name_idx];
      }
      if (det_idx != INT_MAX) {
        if (split[det_idx] != "global") {
          message_info_array_.back().detector =
              geo_mgr_->getDetector(split[det_idx]);
        }
      }
    }
  }
}

void ROOTObjectReaderModule::run(unsigned int event_num) {
  --event_num;
  for (auto &tree : trees_) {
    if (event_num >= tree->GetEntries()) {
      throw EndOfRunException(
          "Requesting end of run because TTree only contains data for " +
          std::to_string(event_num) + " events");
    }
    tree->GetEntry(event_num);
  }
  LOG(TRACE) << "Building messages from stored objects";

  // Loop through all branches
  for (auto message_inf : message_info_array_) {
    auto objects = message_inf.objects;

    // Skip empty objects in current event
    if (objects->empty()) {
      continue;
    }

    // Check if a pointer to a dispatcher method exist
    auto first_object = (*objects)[0];
    auto iter = message_creator_map_.find(typeid(*first_object));
    if (iter == message_creator_map_.end()) {
      LOG(INFO) << "Cannot dispatch message with object "
                << allpix::demangle(typeid(*first_object).name())
                << " because it not registered for messaging";
      continue;
    }

    // Update statistics
    read_cnt_ += objects->size();

    // Create a message
    std::shared_ptr<BaseMessage> message =
        iter->second(*objects, message_inf.detector);

    // Dispatch the message
    messenger_->dispatchMessage(this, message, message_inf.name);
  }
}

void ROOTObjectReaderModule::finalize() {
  int branch_count = 0;
  for (auto &tree : trees_) {
    branch_count += tree->GetListOfBranches()->GetEntries();
  }

  // Print statistics
  LOG(INFO) << "Read " << read_cnt_ << " objects from " << branch_count
            << " branches";

  // Close the file
  input_file_->Close();
}
