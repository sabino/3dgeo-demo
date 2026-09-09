-- Native control/rotation smoke test. Never writes game state or renderer memory.
local symbols = {}
local elf = os.getenv("VIEWER_ELF") or "build/viewer/3dgeo.elf"
local quoted_elf = "'" .. elf:gsub("'", "'\\''") .. "'"
local pipe = assert(io.popen("m68k-neogeo-elf-nm -n " .. quoted_elf))
for line in pipe:lines() do
    local address, name = line:match("^(%x+)%s+%a%s+(%S+)$")
    if address then symbols[name] = tonumber(address, 16) end
end
pipe:close()
local machine = manager.machine
local memory = machine.devices[":maincpu"].spaces["program"]
local screen = machine.screens[":screen"]
local capture_dir = os.getenv("VIEWER_CAPTURE_DIR") or "build/viewer/native-captures"
local names = {"P1 Up", "P1 Down", "P1 Left", "P1 Right", "P1 A", "P1 B", "P1 C", "P1 D"}
local function input(value, select)
    for bit, name in ipairs(names) do
        machine.ioport.ports[":ctrl1:joy:JOY"].fields[name]:set_value((value >> (bit-1)) & 1)
    end
    machine.ioport.ports[":ctrl1:joy:START_SELECT"].fields["P1 Select"]:set_value(select and 1 or 0)
end
local function byte(name) return memory:read_u8(assert(symbols[name], name)) end
local function word(name) return memory:read_u16(assert(symbols[name], name)) end
local function frame_info(label)
    local nonpaper = 0
    local file = assert(io.open(capture_dir .. "/raw-" .. label .. ".ppm", "wb"))
    file:write("P6\n80 56\n255\n")
    for i = 0,4479 do
        local palette = memory:read_u16(symbols.frame_words+i*2) >> 12
        if palette ~= 0 then nonpaper = nonpaper+1 end
        local color = memory:read_u16(0x400000 + (palette*16+1)*2)
        file:write(string.char(((color >> 8)&15)*17, ((color >> 4)&15)*17, (color&15)*17))
    end
    file:close()
    print(string.format("FRAME %s yaw=%d camera=%d pixels=%d", label, byte("viewer_debug_yaw"), word("camera_distance"), nonpaper))
end
local frame, yaw_start, renders_start = 0
local light_before
emu.register_frame_done(function()
    frame = frame + 1
    local keys, select = 0, false
    if frame >= 30 and frame < 40 then keys = 16 end
    if frame >= 100 and frame < 220 then keys = 8 end
    if frame >= 260 and frame < 272 then select = true end
    if frame >= 300 and frame < 312 then keys = 8 end
    if frame >= 330 and frame < 342 then keys = 16 end
    if frame >= 430 and frame < 442 then select = true end
    if frame >= 460 and frame < 472 then keys = 4 end
    if frame >= 490 and frame < 502 then keys = 4 end
    if frame >= 520 and frame < 532 then keys = 16 end
    if frame >= 620 and frame < 680 then keys = 1 | 8 | 16 end
    input(keys, select)
    if frame == 90 then
        assert(byte("viewer_debug_menu") == 0 and byte("viewer_debug_option") == 0, "Cube selection failed")
        screen:snapshot("cube.png")
        frame_info("cube")
    end
    if frame == 100 then yaw_start = byte("viewer_debug_yaw"); renders_start = word("viewer_render_count") end
    if frame == 240 then
        local delta = (byte("viewer_debug_yaw") - yaw_start) & 255
        assert(delta >= 110 and delta <= 130, "Rotation is not tracking 120 VBlank ticks: " .. delta)
        print(string.format("ROTATION PASS ticks=120 angle_delta=%d rendered_frames=%d", delta, word("viewer_render_count")-renders_start))
        screen:snapshot("cube-rotated.png")
    end
    if frame == 400 then
        assert(byte("viewer_debug_menu") == 0 and byte("viewer_debug_option") == 1, "Pyramid selection failed")
        screen:snapshot("pyramid.png")
        frame_info("pyramid")
    end
    if frame == 600 then
        assert(byte("viewer_debug_menu") == 0 and byte("viewer_debug_option") == 7, "Cube light selection failed")
        light_before = {word("scene_light")}
        light_before[2] = memory:read_u16(symbols.scene_light + 2)
        light_before[3] = memory:read_u16(symbols.scene_light + 4)
        screen:snapshot("light-before.png")
    end
    if frame == 740 then
        assert(word("scene_light") ~= light_before[1] and
            memory:read_u16(symbols.scene_light + 2) ~= light_before[2] and
            memory:read_u16(symbols.scene_light + 4) ~= light_before[3],
            "Light controls did not change all three scene_light axes")
        screen:snapshot("light-after.png")
    end
    if frame == 780 then
        assert(word("viewer_vblank_ticks") > 700, "VBlank callback stopped")
        print("VIEWER PASS: cube rotation, pyramid, movable-light controls and VBlank clock")
        machine:exit()
    end
end)
