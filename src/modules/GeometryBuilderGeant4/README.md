# GeometryBuilderGeant4
**Maintainer**: Koen Wolters (<koen.wolters@cern.ch>)  
**Status**: Functional  

### Description
Constructs the Geant4 geometry from the internal geometry description. First constructs the world frame with a configurable margin and material. Then continues to create all the detectors using their internal detector models and to place them within the world frame.

All available detector models are fully supported. This builder can create extra support layers of the following materials:

* Air
* Aluminum
* Carbonfiber (a mixture of carbon and epoxy)
* Copper
* Epoxy
* G10 (PCB material)
* Kapton (using the `G4_KAPTON` definition)
* Lead
* Plexiglass (using the `G4_PLEXIGLASS` definition)
* Silicon
* Solder (a mixture of tin and lead)
* Tungsten

### Dependencies

This module requires an installation Geant4.

### Parameters
* `world_material` : Material of the world, should either be **air** or **vacuum**. Defaults to **air** if not specified.
* `world_margin_percentage` : Percentage of the world size to add to every dimension compared to the internally calculated minimum world size. Defaults to 0.1, thus 10%.
* `world_minimum_margin` : Minimum absolute margin to add to all sides of the internally calculated minimum world size. Defaults to zero for all axis, thus not requiring any minimum margin.
* `GDML_output_file` : Optional file to write the geometry to in GDML format. Can only be used if this Geant4 version is built with GDML support enabled and will throw an error otherwise. This feature is to be considered experimental as the GDML implementation of Geant4 is incomplete.
* `GDML_input_file` : Optional argument to import the geometry of one or multiple GDML files. Like in `GDML_output_file`, the GDML support in Geant4 has to be enabled. Please notice that the world size needs to be large enough to include the new geometry.
* `GDML_input_offset' : Optional position arguments to the imported GDML geometries. Specified as an array of three dimensional values. Its size has to be equal to the number of imported files. If no offset is specified, the geometries are placed at the origin.

### Usage
To create a Geant4 geometry using vacuum as world material and with always exactly one meter added to the minimum world size in every dimension, the following configuration could be used:

```ini
[GeometryBuilderGeant4]
world_material = "vacuum"
world_margin_percentage = 0
world_minimum_margin = 1m 1m 1m
```
To import multiple GDML files, the file names are placed next to each other like in the example configuration below:

```ini
[GeometryBuilderGeant4]
GDML_input_file = "assembly.gdml" "assembly2.gdml"
GDML_input_offset = [[-1cm 0cm 0cm], [0 1cm 0cm]]
```
