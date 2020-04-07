#!ls
# C:\Projects\LabSoundGraphToy\resources\sample_patch.ls
node: Oscillatorname: Oscillator-2
 pos: 372 415
 setting: type Enumeration Sine
 param: frequency 440.000000
 param: amplitude 1.000000
 param: bias 0.000000
 param: detune 0.000000
node: Oscillatorname: Oscillator-1
 pos: 257 197
 setting: type Enumeration Sine
 param: frequency 2.000000
 param: amplitude 20.000000
 param: bias 0.000000
 param: detune 0.000000
node: Devicename: Device-1
 pos: 892 368.5
 + Oscillator-2: -> Device-1:Device-1
 + Oscillator-1: -> Oscillator-2:Oscillator-2
