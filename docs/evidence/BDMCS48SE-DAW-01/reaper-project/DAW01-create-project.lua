local midi = reaper.GetTrack(0, 0)
local instrument = reaper.GetTrack(0, 1)
local capture = reaper.GetTrack(0, 2)
if not midi or not instrument or not capture then
  reaper.ShowConsoleMsg("ERROR DAW-01: starter project did not load expected tracks\n")
  return
end

-- REAPER's named-FX insertion API searches relative to the selected track.
reaper.SetOnlyTrackSelected(instrument)
local fx = reaper.TrackFX_AddByName(instrument, "VST3: BDMCS48SE Instrument (ABDMCS48SE)", false, 1)
if fx < 0 then
  fx = reaper.TrackFX_AddByName(instrument, "VST3: BDMCS48SE Instrument", false, 1)
end
if fx < 0 then
  reaper.ShowConsoleMsg("ERROR DAW-01: VST3 not instantiated; check REAPER VST scan\n")
  return
end

reaper.SetMediaTrackInfo_Value(instrument, "I_RECARM", 1)
reaper.SetMediaTrackInfo_Value(instrument, "I_RECMON", 1)
reaper.SetMediaTrackInfo_Value(capture, "I_RECMODE", 3)
reaper.SetMediaTrackInfo_Value(capture, "I_RECARM", 0)
reaper.SetMediaTrackInfo_Value(capture, "I_RECMON", 0)

-- Send the MIDI item to the instrument, but not to the capture track.
local midiSend = reaper.CreateTrackSend(midi, instrument)
if midiSend < 0 then
  reaper.ShowConsoleMsg("ERROR DAW-01: could not route MIDI track to instrument\n")
  return
end
reaper.SetTrackSendInfo_Value(midi, 0, midiSend, "I_SRCCHAN", -1)
reaper.SetTrackSendInfo_Value(midi, 0, midiSend, "I_DSTCHAN", 0)
reaper.SetTrackSendInfo_Value(midi, 0, midiSend, "I_MIDIFLAGS", 0)

-- Send stereo instrument audio to capture, with MIDI disabled on this send.
local audioSend = reaper.CreateTrackSend(instrument, capture)
if audioSend < 0 then
  reaper.ShowConsoleMsg("ERROR DAW-01: could not route instrument output to capture track\n")
  return
end
reaper.SetTrackSendInfo_Value(instrument, 0, audioSend, "I_SRCCHAN", 0)
reaper.SetTrackSendInfo_Value(instrument, 0, audioSend, "I_DSTCHAN", 0)
reaper.SetTrackSendInfo_Value(instrument, 0, audioSend, "I_MIDIFLAGS", 31)
reaper.SetTrackSendInfo_Value(instrument, 0, audioSend, "D_VOL", 1.0)
reaper.SetTrackSendInfo_Value(instrument, 0, audioSend, "D_PAN", 0.0)

local ok, name = reaper.TrackFX_GetFXName(instrument, fx, "")
reaper.Main_SaveProjectEx(reaper.EnumProjects(-1), "", 0)
reaper.ShowConsoleMsg("DAW-01 VST3 inserted on instrument track: " .. tostring(ok) .. " / " .. tostring(name) .. "\n")
reaper.ShowConsoleMsg("MIDI send to instrument and stereo audio send to capture track created. Verify the active audio device before rendering.\n")
