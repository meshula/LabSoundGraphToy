#include "OSCNode.hpp"
#include "LabSound/extended/Registry.h"
bool OSCNode::s_registered = lab::NodeRegistry::Register(OSCNode::static_name(),
    [](lab::AudioContext& ac)->lab::AudioNode* { return new OSCNode(ac); },
    [](lab::AudioNode* n) { delete n; });
