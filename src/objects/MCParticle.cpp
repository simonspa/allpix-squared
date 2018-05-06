/**
 * @file
 * @brief Implementation of Monte-Carlo particle object
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied
 * verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities
 * granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#include "MCParticle.hpp"
#include <sstream>

using namespace allpix;

MCParticle::MCParticle(ROOT::Math::XYZPoint local_start_point,
                       ROOT::Math::XYZPoint global_start_point,
                       ROOT::Math::XYZPoint local_end_point,
                       ROOT::Math::XYZPoint global_end_point, int particle_id)
    : local_start_point_(std::move(local_start_point)),
      global_start_point_(std::move(global_start_point)),
      local_end_point_(std::move(local_end_point)),
      global_end_point_(std::move(global_end_point)),
      particle_id_(particle_id) {
  setParent(nullptr);
  setTrack(nullptr);
}

ROOT::Math::XYZPoint MCParticle::getLocalStartPoint() const {
  return local_start_point_;
}
ROOT::Math::XYZPoint MCParticle::getGlobalStartPoint() const {
  return global_start_point_;
}

ROOT::Math::XYZPoint MCParticle::getLocalEndPoint() const {
  return local_end_point_;
}
ROOT::Math::XYZPoint MCParticle::getGlobalEndPoint() const {
  return global_end_point_;
}

int MCParticle::getParticleID() const { return particle_id_; }

void MCParticle::setParent(const MCParticle *mc_particle) {
  parent_ = const_cast<MCParticle *>(mc_particle); // NOLINT
}

/**
 * Object is stored as TRef and can only be accessed if pointed object is in
 * scope
 */
const MCParticle *MCParticle::getParent() const {
  return dynamic_cast<MCParticle *>(parent_.GetObject());
}

void MCParticle::setTrack(const MCTrack *mc_track) {
  track_ = const_cast<MCTrack *>(mc_track); // NOLINT
}

/**
 * Object is stored as TRef and can only be accessed if pointed object is in
 * scope
 */
const MCTrack *MCParticle::getTrack() const {
  return dynamic_cast<MCTrack *>(track_.GetObject());
}

void MCParticle::print(std::ostream &out) const {
  static const size_t big_gap = 25;
  static const size_t med_gap = 10;
  static const size_t small_gap = 6;
  static const size_t largest_output = big_gap + 3 * med_gap + 3 * small_gap;

  auto track = getTrack();
  auto parent = getParent();

  auto title = std::stringstream();
  title << "--- Printing MCParticle information (" << this << ") ";
  out << '\n'
      << std::setw(largest_output) << std::left << std::setfill('-')
      << title.str() << '\n'
      << std::setfill(' ') << std::left << std::setw(big_gap)
      << "Particle type (PDG ID): " << std::right << std::setw(small_gap)
      << particle_id_ << '\n'
      << std::left << std::setw(big_gap) << "Local start point:" << std::right
      << std::setw(med_gap) << local_start_point_.X() << std::setw(small_gap)
      << " mm |" << std::setw(med_gap) << local_start_point_.Y()
      << std::setw(small_gap) << " mm |" << std::setw(med_gap)
      << local_start_point_.Z() << std::setw(small_gap) << " mm  \n"
      << std::left << std::setw(big_gap) << "Global start point:" << std::right
      << std::setw(med_gap) << global_start_point_.X() << std::setw(small_gap)
      << " mm |" << std::setw(med_gap) << global_start_point_.Y()
      << std::setw(small_gap) << " mm |" << std::setw(med_gap)
      << global_start_point_.Z() << std::setw(small_gap) << " mm  \n"
      << std::left << std::setw(big_gap) << "Local end point:" << std::right
      << std::setw(med_gap) << local_end_point_.X() << std::setw(small_gap)
      << " mm |" << std::setw(med_gap) << local_end_point_.Y()
      << std::setw(small_gap) << " mm |" << std::setw(med_gap)
      << local_end_point_.Z() << std::setw(small_gap) << " mm  \n"
      << std::left << std::setw(big_gap) << "Global end point:" << std::right
      << std::setw(med_gap) << global_end_point_.X() << std::setw(small_gap)
      << " mm |" << std::setw(med_gap) << global_end_point_.Y()
      << std::setw(small_gap) << " mm |" << std::setw(med_gap)
      << global_end_point_.Z() << std::setw(small_gap) << " mm  \n"
      << std::left << std::setw(big_gap) << "Linked parent:";
  if (parent != nullptr) {
    out << std::right << std::setw(small_gap) << parent << '\n';
  } else {
    out << std::right << std::setw(small_gap) << "<nullptr>\n";
  }
  out << std::left << std::setw(big_gap) << "Linked track:";
  if (track != nullptr) {
    out << std::right << std::setw(small_gap) << track << '\n';
  } else {
    out << std::right << std::setw(small_gap) << "<nullptr>\n";
  }
  out << std::setfill('-') << std::setw(largest_output) << ""
      << std::setfill(' ') << std::endl;
}
