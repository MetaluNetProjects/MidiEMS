# MidiEMS
MIDI controlled electrical muscle stimulator

for Boris Gibé / Les Choses de Rien

MIDI configuration:

- listen to MIDI Channel 9 only
- note_on 1-8 with non-null velocity: start EMS channel 1-8 with velocity applied to pulse width
- note_off or zero-velocity note_on 1-8: stop EMS channel 1-8. Note a channel automatically stops 2 seconds after last note_on.
- control change 1-8: set the pulse repetition frequency in Hertz for EMS channel 1-8
- control change 11-18: if not null, force the pulse width for EMS channel 1-8 (replace the velocity)
- control change 20: if not null, global dimm for pulses width: all pulses width are multiplied by (ctl20 + 1) / 128

(c) metalu.net 2022
