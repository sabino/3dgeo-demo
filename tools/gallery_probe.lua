-- SPDX-License-Identifier: Apache-2.0
-- Documentation capture: real controller input only, no writes to game RAM.
local symbols = {}
local elf = assert(os.getenv("VIEWER_ELF"))
local pipe = assert(io.popen("m68k-neogeo-elf-nm -n '" .. elf:gsub("'", "'\\''") .. "'"))
for line in pipe:lines() do
    local address, name = line:match("^(%x+)%s+%a%s+(%S+)$")
    if address then symbols[name] = tonumber(address, 16) end
end
assert(pipe:close())
local machine = manager.machine
local memory = machine.devices[":maincpu"].spaces["program"]
local screen = machine.screens[":screen"]
local names = {"cube", "pyramid", "tetrahedron", "ball", "donut", "cone", "cylinder", "light"}
local buttons = {"P1 Up", "P1 Down", "P1 Left", "P1 Right", "P1 A", "P1 B", "P1 C", "P1 D"}
local function byte(name) return memory:read_u8(assert(symbols[name], name)) end
local function input(value, select)
    for bit, name in ipairs(buttons) do
        machine.ioport.ports[":ctrl1:joy:JOY"].fields[name]:set_value((value >> (bit-1)) & 1)
    end
    machine.ioport.ports[":ctrl1:joy:START_SELECT"].fields["P1 Select"]:set_value(select and 1 or 0)
end
local frame = 0
emu.register_frame_done(function()
    frame = frame + 1
    local model = math.floor(frame / 240)
    local phase = frame % 240
    local keys, select = 0, false
    if model < 8 then
        if model > 0 and phase >= 5 and phase < 21 then keys = 8 end
        if phase >= 40 and phase < 56 then keys = 16 end
        -- Rotate the pyramid away from its base; angles carry into later scenes.
        if model == 1 and phase >= 60 and phase < 124 then keys = 4 end
        -- Bring the light marker inside the viewport without modifying RAM.
        if model == 7 and phase >= 70 and phase < 94 then keys = 4 | 2 end
        if phase == 130 then
            assert(byte("viewer_debug_menu") == 0, "Gallery did not enter model")
            assert(byte("viewer_debug_option") == model, "Gallery model selection mismatch")
            screen:snapshot(names[model + 1] .. ".png")
            print(string.format("CAPTURE model=%s frame=%d yaw=%d", names[model + 1], frame, byte("viewer_debug_yaw")))
        end
        if phase >= 165 and phase < 181 then select = true end
    end
    input(keys, select)
    if frame == 1900 then
        print("GALLERY PASS: eight native model/lighting screenshots")
        machine:exit()
    end
end)
