-- End-to-end test driven by mGBA's scripting engine. Launched by e2e.mjs, which
-- fills in the two placeholders below and answers the challenge using the
-- server's reference implementation.
local OUT = "__OUT__"
local USERNAME = "__USERNAME__"

local SRAM = 0x0E000000
local STATE_OFFSET = 12
local CHALLENGE_OFFSET = 31

local USERNAME_ROWS = { "1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm_" }
local CODE_ROWS = { "0123456789", "ABCDEFGHJK", "MNPQRSTVWX", "YZ" }

local function wait(frames)
    for _ = 1, frames do coroutine.yield() end
end

local function press(key)
    emu:addKey(key)
    wait(3)
    emu:clearKey(key)
    wait(5)
end

local function read_file(name)
    local file = io.open(OUT .. "/" .. name, "r")
    if not file then return nil end
    local text = file:read("a")
    file:close()
    return text
end

local function write_file(name, text)
    local file = assert(io.open(OUT .. "/" .. name, "w"))
    file:write(text)
    file:close()
end

local function shot(name)
    wait(4)
    emu:screenshot(OUT .. "/" .. name .. ".png")
end

local function state()
    return emu:read8(SRAM + STATE_OFFSET)
end

local function challenge()
    local text = ""
    for index = 0, 7 do
        text = text .. string.char(emu:read8(SRAM + CHALLENGE_OFFSET + index))
    end
    return text
end

-- Mirrors the cursor rules of run_keyboard() in src/main.cpp.
local function type_text(rows, text)
    local row, column = 2, 1
    for index = 1, #text do
        local wanted = text:sub(index, index)
        local target_row, target_column
        for r = 1, #rows do
            local found = rows[r]:find(wanted, 1, true)
            if found then target_row, target_column = r, found end
        end
        assert(target_row, "no key for " .. wanted)

        while row ~= target_row do
            press(C.GBA_KEY.DOWN)
            row = row % #rows + 1
            if column > #rows[row] then column = #rows[row] end
        end
        while column ~= target_column do
            press(C.GBA_KEY.RIGHT)
            column = column % #rows[row] + 1
        end
        press(C.GBA_KEY.A)
    end
end

local function expect(condition, message)
    if not condition then error(message, 0) end
end

local function run()
    wait(90)
    expect(state() == 0, "expected signed-out state at boot, got " .. state())
    shot("1-sign-in")

    type_text(USERNAME_ROWS, USERNAME)
    shot("2-username")
    press(C.GBA_KEY.START)
    wait(60)
    expect(state() == 1, "expected pending state after START, got " .. state())
    shot("3-challenge")

    write_file("challenge.txt", challenge())
    local code
    for _ = 1, 1200 do
        code = read_file("code.txt")
        if code and #code >= 10 then break end
        wait(1)
    end
    expect(code and #code >= 10, "timed out waiting for code.txt")
    code = code:sub(1, 10)

    press(C.GBA_KEY.A)
    wait(20)
    shot("4-code-empty")

    -- A wrong code must be rejected and must not sign the user in.
    local wrong = (code:sub(1, 1) == "0" and "1" or "0") .. code:sub(2)
    type_text(CODE_ROWS, wrong)
    wait(30)
    expect(state() == 1, "wrong code changed the state to " .. state())
    shot("5-wrong-code")

    type_text(CODE_ROWS, code)
    wait(40)
    expect(state() == 2, "right code did not sign in, state " .. state())
    shot("6-profile")

    press(C.GBA_KEY.A)
    wait(40)
    expect(state() == 0, "sign out did not reset the state, got " .. state())
    shot("7-signed-out")
end

local routine = coroutine.create(run)
local finished = false

callbacks:add("frame", function()
    if finished then return end
    local ok, message = coroutine.resume(routine)
    if not ok then
        finished = true
        write_file("result.txt", "FAIL: " .. tostring(message))
    elseif coroutine.status(routine) == "dead" then
        finished = true
        write_file("result.txt", "PASS")
    end
end)
