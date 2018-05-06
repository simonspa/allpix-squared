/**
 * @file
 * @brief Defines a concrete implementation of a G4VUserTrackInformation to
 * carry unique track and parent track IDs as well
 * as create and dispatch MCTracks which are the AP2 representation of a
 * Monte-Carlo trajectory
 * @copyright Copyright (c) 2018 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied
 * verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities
 * granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#ifndef TrackInfoG4_H
#define TrackInfoG4_H 1

#include <map>

#include "G4Track.hh"
#include "G4VUserTrackInformation.hh"

#include "core/messenger/Messenger.hpp"
#include "objects/MCTrack.hpp"

namespace allpix {
/**
 * @brief Allpix implementation of G4VUserTrackInformation to handle unique
 * track IDs and the creation of MCTracks
 */
class TrackInfoG4 : public G4VUserTrackInformation {
public:
  /**
   * @brief Only available constructor
   * @param custom_track_id The custom id for this track
   * @param parent_track_id The custom id of parent track
   * @param aTrack The Geant4 track
   */
  TrackInfoG4(int custom_track_id, int parent_track_id,
              const G4Track *const aTrack);

  /**
   * @brief Getter for custom id of track
   * @return The custom track id
   */
  int getID() const;

  /**
   * @brief Getter for parent's track id of track
   * @return The parent's custrom track id
   */
  int getParentID() const;

  /**
   * @brief Update track info from the G4Track
   * @param aTrack A pointer to a G4Track instance which represents this track's
   * final state
   */
  void finalizeInfo(const G4Track *const aTrack);

  /**
   * @brief Get the point where the track originated in global coordinates
   * @return Track start point
   */
  ROOT::Math::XYZPoint getStartPoint() const;

  /**
   * @brief Get the point of where the track terminated in global coordiantes
   * @return Track end point
   */
  ROOT::Math::XYZPoint getEndPoint() const;

  /**
   * @brief Get PDG particle id for the particle
   * @return Particle id
   */
  int getParticleID() const;

  /**
   * @brief Get the Geant4 internal ID of the process which created the particle
   * @return The Geant4 process type, or "-1" if no such process exists
   */
  int getCreationProcessType() const;

  /**
   * @brief Get the amount of stepping steps the Geant4 track made
   * @return The amount of steps
   */
  int getNumberOfSteps() const;

  /**
   * @brief Getter for the kinetic energy the particle had when the track was
   * created
   * @return The kinetic energy in MeV of the particle at the beginning of the
   * track
   */
  double getKineticEnergyInitial() const;
  /**
   * @brief Getter for the total energy (i.e. kinetic energy and dynamic mass)
   * the particle had when the track was
   * created
   * @return The total energy in MeV of the particle at the beginning of the
   * track
   */
  double getTotalEnergyInitial() const;

  /**
   * @brief Getter for the kinetic energy the particle had when the track
   * terminated
   * @return The kinetic energy in MeV of the particle at the end of the track
   */
  double getKineticEnergyFinal() const;

  /**
   * @brief Getter for the total energy (i.e. kinetic energy and dynamic mass)
   * the particle had when the track
   * terminated
   * @return The total energy in MeV of the particle at the end of the track
   */
  double getTotalEnergyFinal() const;

  /**
   * @brief Getter for the Geant4 name of the physical volume in which the track
   * originated
   * @return The name of the phyical volume
   */
  std::string getOriginatingVolumeName() const;

  /**
   * @brief Getter for the name of the process which created this particle
   * @return The process name, or "none" if no such process exists
   */
  std::string getCreationProcessName() const;

private:
  // Assigned track id to track
  int custom_track_id_{};
  // Parent's track id
  int parent_track_id_{};
  // Geant4 Type of the process which created this track
  int origin_g4_process_type_{};
  // PDG particle id
  int particle_id_{};
  // Number of steps this track made in Geant4
  int n_steps_{};
  // Start point of track (in mm)
  ROOT::Math::XYZPoint start_point_{};
  // End point of track (in mm)
  ROOT::Math::XYZPoint end_point_{};
  // Geant4 volume in which the track was created
  std::string origin_g4_vol_name_{};
  // Name of Geant4 process which created this track
  std::string origin_g4_process_name_{};
  // Initial kinetic energy (MeV)
  double initial_kin_E_{};
  // Initial total energy (MeV)
  double initial_tot_E_{};
  // Final kinetic energy (MeV)
  double final_kin_E_{};
  // Final total energy (MeV)
  double final_tot_E_{};
};
} // namespace allpix
#endif
