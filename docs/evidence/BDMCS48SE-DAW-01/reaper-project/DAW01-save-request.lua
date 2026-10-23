local project = reaper.EnumProjects(-1)
local path = "D:/desarrollos/ABDSynths/ABDMCS48SE/docs/evidence/BDMCS48SE-DAW-01/reaper-project/BDMCS48SE-DAW-01.RPP"

-- Main_SaveProjectEx returns void. The API's option bit 8 changes this
-- ReaProject's filename when saving to the explicit path.
reaper.Main_SaveProjectEx(project, path, 8)
local _, savedPath = reaper.EnumProjects(-1, "")
local title = reaper.GetProjectName(project, "")
reaper.ShowConsoleMsg("DAW-01 Save As requested: " .. tostring(title) .. " / " .. tostring(savedPath) .. "\n")
