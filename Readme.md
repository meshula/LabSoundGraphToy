
# LabSound GraphToy

Copyright 2020 Nick Porcino

License: MIT

The submodules are licensed under their respective terms.

## LabSound GraphToy has moved here: https://github.com/LabSound/LabSoundGraphToy

**LabSound GraphToy** is a workspace for exploring **LabSound** nodes and their properties.

![Example](resources/preview.png)

Right click on the canvas to create new nodes. Drag wires from outputs to inputs. 
Click the fields to edit the parameters and settings.

Some nodes, such as the Oscillator must be started before they make sound. 
These nodes will have a "play" icon near their name which can be clicked to activate them.

Click a node's name to reveal a control that allows deleting a node.
Click a wire to reveal an option to delete the connection.

Click and drag on an empty area in a node to move it around.

Mouse scroll zooms in and out at the cursor point.

Group nodes can be used to draw boxes around other nodes. They also contain nodes
subsequently created. This functionality is a bit rough, because there's no way to
go back to adding nodes to the global canvas once you've got a group node. 

## Getting Started

````sh
cd {source-location}
git clone --recursive {this-repo}
cd {build-location}
cmake -G {generator} -DCMAKE_INSTALL_PREFIX={install-location} {source-location}
{run build and install commands}
````

## In Progress

- [ ] Add a delete icon (red circle-x)
- [ ] Replace delete dialog boxes with red-circle-x that appears when the cursor hovers in an appropriate spot
- [ ] Add group selection
- [ ] Make canvas recursive, with left edge ins, and right edge outs
- [ ] Add function to turn group selection into a subgraph node
- [ ] Add expand subgraph canvas, and collapse. Levels of recursion visible as bars with in/out wiring on left and right
- [ ] Add breadcrumb in status bar to indicate canvas recursion level
- [ ] Create a viable save file format
- [ ] Load and parse a saved file
- [ ] Implement File/New

## Credits

**LabSound** [https://github.com/LabSound/LabSound](https://github.com/LabSound/LabSound)

**ImGui** [https://github.com/ocornut/imgui.git](https://github.com/ocornut/imgui.git)

**Sokol** [https://github.com/floooh/sokol.git](https://github.com/floooh/sokol.git)

