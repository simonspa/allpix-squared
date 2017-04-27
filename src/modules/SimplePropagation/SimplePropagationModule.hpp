/**
 * @author Paul Schuetze <paul.schuetze@desy.de>
 * @author Koen Wolters <koen.wolters@cern.ch>
 */

#include <memory>
#include <random>
#include <string>
#include <vector>

#include <Math/Point3D.h>
#include <TFile.h>

#include "core/config/Configuration.hpp"
#include "core/geometry/DetectorModel.hpp"
#include "core/messenger/Messenger.hpp"
#include "core/module/Module.hpp"

#include "objects/DepositedCharge.hpp"

namespace allpix {
    // define the module to inherit from the module base class
    class SimplePropagationModule : public Module {
    public:
        // constructor and destructor
        SimplePropagationModule(Configuration, Messenger*, std::shared_ptr<Detector>);
        ~SimplePropagationModule() override;

        // init debug plots
        void init() override;

        // do the propagation of the charge deposits
        void run(unsigned int event_num) override;

        // finalize debug plots
        void finalize() override;

    private:
        // propagate a single charge
        std::pair<ROOT::Math::XYZPoint, double> propagate(const ROOT::Math::XYZPoint& pos);

        // random generator for this module
        std::mt19937_64 random_generator_;

        // configuration for this module
        Configuration config_;

        // pointer to the messenger
        Messenger* messenger_;

        // attached detector and detector model
        std::shared_ptr<Detector> detector_;
        std::shared_ptr<DetectorModel> model_;

        // deposits for a specific detector
        std::shared_ptr<DepositedChargeMessage> deposits_message_;

        // debug list of points to plot
        std::vector<std::vector<ROOT::Math::XYZPoint>> debug_plot_points_;
        TFile* debug_file_;
    };

} // namespace allpix
