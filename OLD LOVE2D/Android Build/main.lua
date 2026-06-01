local Backend = require("Backend")

local SCOPE_FL = 900
local EYEPIECE_FL = 9
local EYEPIECE_AFOV = 66
app_mode = "log" 

local planets_data = {}
local queue = {}
local is_fetching = false
local time_offset = 0
local selected_planet = "Jupiter"
local night_mode = false
local loc_status = "Locating..."

-- Core loading screen state flag tracking variable
local is_initializing = true

local scroll_y = 0
local max_scroll = 0

local journal_scroll_y = 0
local journal_max_scroll = 0

local active_input = nil 
local custom_name = ""
local custom_id = ""

local log_notes = ""
local saved_logs = {}

-- Variables for our custom location settings
local loc_address = ""
local loc_city = ""
local loc_country = ""
local is_updating_loc = false

local idle_time = 0
local is_dimmed = false

local hold_timers = { left = 0, right = 0, plus = 0, minus = 0 }
local HOLD_SPEED = 0.15 

-- Mobile explicit touch-dragging scrolling metrics variables
local is_dragging_touch = false
local touch_start_y = 0
local scroll_start_y = 0

-- Helper function to drop the last full UTF-8 character safely without breaking byte strings
local function utf8_pop(str)
    return str:gsub("[%z\x01-\x7F\xC2-\xF4][\x80-\xBF]*$", "")
end

-- Helper function to save your modified coordinates/text configuration safely to disk
local function save_location_profile()
    local data_string = string.format("%f\n%f\n%s\n%s\n%s", 
        Backend.lat, Backend.lon, loc_address, loc_city, loc_country)
    love.filesystem.write("custom_location.txt", data_string)
end

-- Forward declaration of queue helpers so they link together perfectly
local process_next_in_queue

local function refresh_data()
    if is_fetching then return end
    queue = {}
    for i, p in ipairs(Backend.planets) do 
        table.insert(queue, p) 
    end
    is_fetching = true
    process_next_in_queue()
end

process_next_in_queue = function()
    if #queue == 0 then 
        is_fetching = false 
        if is_initializing then
            is_initializing = false
        end
        return 
    end
    local planet = table.remove(queue, 1)
    Backend.fetch_planet(planet, os.time() + time_offset, function(data, err)
        if data then 
            planets_data[data.planet.name] = data 
        end
        process_next_in_queue()
    end)
end

-- Helper function to fetch coordinates using OpenStreetMap Nominatim API asynchronously
local function geocode_address()
    if is_updating_loc then return end
    is_updating_loc = true
    local old_status = loc_status
    loc_status = "Geocoding Address..."
    
    local query_string = string.format("%s, %s, %s", loc_address, loc_city, loc_country)
    local encoded_query = query_string:gsub("([^%w])", function(c)
        return string.format("%%%02X", string.byte(c))
    end)
    
    local url = "https://nominatim.openstreetmap.org/search?q=" .. encoded_query .. "&format=json&limit=1"
    
    local thread_code = [[
        local url = ...
        local cmd = string.format('curl -s -A "EphemerisLiveObserver/1.0" -L "%s"', url)
        local handle = io.popen(cmd)
        local result = handle:read("*a")
        handle:close()
        
        if result then
            local lat = result:match('"lat":"([%d%.%-]+)"')
            local lon = result:match('"lon":"([%d%.%-]+)"')
            if lat and lon then
                love.thread.getChannel("geo_res"):push({lat = tonumber(lat), lon = tonumber(lon)})
                return
            end
        end
        love.thread.getChannel("geo_res"):push({error = true})
    ]]
    
    love.thread.newThread(thread_code):start(url)
end

function love.load()
    -- Safe loading for mobile distributions: wrap in pcall in case icon assets are missing from build package
    pcall(function()
        local iconData = love.image.newImageData("icon.png")
        love.window.setIcon(iconData)
    end)

    love.window.setTitle("Ephemeris Live")
    -- Resizable configuration is preserved so standard scaling applies automatically
    love.window.setMode(1200, 800, {resizable=true})
    
    font_bold = love.graphics.newFont(16)
    font_small = love.graphics.newFont(12)
    font_tiny = love.graphics.newFont(10)
    
    if love.filesystem.getInfo("observer_journal.txt") then
        for line in love.filesystem.lines("observer_journal.txt") do
            table.insert(saved_logs, line)
        end
    end
    
    if love.filesystem.getInfo("custom_location.txt") then
        local lines = {}
        for line in love.filesystem.lines("custom_location.txt") do
            table.insert(lines, line)
        end
        if #lines >= 5 then
            Backend.lat = tonumber(lines[1]) or Backend.lat
            Backend.lon = tonumber(lines[2]) or Backend.lon
            loc_address = lines[3] or ""
            loc_city = lines[4] or ""
            loc_country = lines[5] or ""
            loc_status = string.format("Lat: %.2f Lon: %.2f", Backend.lat, Backend.lon)
            refresh_data()
        end
    else
        Backend.initLocation()
    end
end

function love.update(dt) 
    Backend.poll() 

    idle_time = idle_time + dt
    if idle_time > 120 then 
        is_dimmed = true 
    end

    if Backend.updateLocation() then
        loc_status = string.format("Lat: %.2f Lon: %.2f", Backend.lat, Backend.lon)
        refresh_data() 
    end
    
    local geo_msg = love.thread.getChannel("geo_res"):pop()
    if geo_msg then
        is_updating_loc = false
        if geo_msg.error then
            loc_status = "Geocode Failed. Retrying..."
        else
            Backend.lat = geo_msg.lat
            Backend.lon = geo_msg.lon
            loc_status = string.format("Lat: %.2f Lon: %.2f", Backend.lat, Backend.lon)
            save_location_profile()
            refresh_data()
        end
    end

    if is_initializing then return end

    local trigger_refresh = false
    
    if love.keyboard.isDown("right") then
        hold_timers.right = hold_timers.right - dt
        if hold_timers.right <= 0 then
            time_offset = time_offset + 3600
            trigger_refresh = true
            hold_timers.right = HOLD_SPEED
        end
    else hold_timers.right = 0 end

    if love.keyboard.isDown("left") then
        hold_timers.left = hold_timers.left - dt
        if hold_timers.left <= 0 then
            time_offset = time_offset - 3600
            trigger_refresh = true
            hold_timers.left = HOLD_SPEED
        end
    else hold_timers.left = 0 end

    if not active_input then
        if love.keyboard.isDown("=", "kp+") then
            hold_timers.plus = hold_timers.plus - dt
            if hold_timers.plus <= 0 then
                EYEPIECE_FL = EYEPIECE_FL + 1
                hold_timers.plus = HOLD_SPEED
            end
        else hold_timers.plus = 0 end

        if love.keyboard.isDown(")", "kp-") then
            hold_timers.minus = hold_timers.minus - dt
            if hold_timers.minus <= 0 then
                EYEPIECE_FL = math.max(1, EYEPIECE_FL - 1)
                hold_timers.minus = HOLD_SPEED
            end
        else hold_timers.minus = 0 end
    end

    if trigger_refresh then 
        refresh_data() 
    end
end

function love.mousemoved(x, y, dx, dy)
    idle_time = 0
    is_dimmed = false
end

function love.wheelmoved(x, y)
    if is_initializing then return end
    idle_time = 0
    is_dimmed = false
    local w, h = love.graphics.getDimensions()
    local list_w = w > h and math.max(240, math.floor(w * 0.25)) or w

    if w > h and love.mouse.getX() < list_w then
        if app_mode == "log" then
            scroll_y = scroll_y - (y * 30)
            scroll_y = math.max(0, math.min(scroll_y, max_scroll))
        elseif app_mode == "journal" then
            journal_scroll_y = journal_scroll_y - (y * 30)
            journal_scroll_y = math.max(0, math.min(journal_scroll_y, journal_max_scroll))
        end
    elseif h >= w and love.mouse.getY() > (h * 0.40) then
        -- Vertical stacking scroll adjustments
        if app_mode == "log" then
            scroll_y = scroll_y - (y * 30)
            scroll_y = math.max(0, math.min(scroll_y, max_scroll))
        elseif app_mode == "journal" then
            journal_scroll_y = journal_scroll_y - (y * 30)
            journal_scroll_y = math.max(0, math.min(journal_scroll_y, journal_max_scroll))
        end
    end
end

-- Native touch-dragging emulation handlers to support sleek scroll mechanics on mobile
function love.touchpressed(id, x, y, dx, dy, pressure)
    idle_time = 0
    is_dimmed = false
    local win_w, win_h = love.graphics.getDimensions()
    
    -- Translate relative screen floats (0.0 - 1.0) into structural scaling pixels
    local px, py = x * win_w, y * win_h
    is_dragging_touch = true
    touch_start_y = py
    scroll_start_y = (app_mode == "journal") and journal_scroll_y or scroll_y
end

function love.touchmoved(id, x, y, dx, dy, pressure)
    if not is_dragging_touch then return end
    local win_w, win_h = love.graphics.getDimensions()
    local py = y * win_h
    local delta_y = py - touch_start_y

    if app_mode == "log" then
        scroll_y = math.max(0, math.min(scroll_start_y - delta_y, max_scroll))
    elseif app_mode == "journal" then
        journal_scroll_y = math.max(0, math.min(scroll_start_y - delta_y, journal_max_scroll))
    end
end

function love.touchreleased(id, x, y, dx, dy, pressure)
    is_dragging_touch = false
end

local function set_color(r, g, b, a)
    if night_mode then
        local luminance = (r + g + b) / 3
        love.graphics.setColor(luminance, 0, 0, a or 1)
    else
        love.graphics.setColor(r, g, b, a or 1)
    end
end

function get_map_coords(alt, az, centerX, centerY, radius)
    if alt < 0 then return nil, nil end
    local r = radius * (1 - (alt / 90))
    local theta = math.rad(az - 90)
    return centerX + r * math.cos(theta), centerY + r * math.sin(theta)
end

function love.draw()
    local w, h = love.graphics.getDimensions()

    if is_initializing then
        love.graphics.clear(0.01, 0.01, 0.03)
        love.graphics.push("all")
        
        set_color(0.92, 0.72, 0.22, 1)
        love.graphics.setFont(font_bold)
        love.graphics.printf("EPHEMERIS LIVE", 0, h / 2 - 80, w, "center")
        
        set_color(0.4, 0.4, 0.6, 1)
        love.graphics.setFont(font_small)
        love.graphics.printf("INITIALIZING OBSERVATION DECK SYSTEMS...", 0, h / 2 - 45, w, "center")
        
        local bar_w = math.min(320, w - 60)
        local bar_h = 6
        local bar_x = (w - bar_w) / 2
        local bar_y = h / 2 - 20
        
        set_color(0.1, 0.1, 0.2, 1)
        love.graphics.rectangle("fill", bar_x, bar_y, bar_w, bar_h, 3)
        
        local total_targets = #Backend.planets
        local pending_count = #queue
        local completed_targets = total_targets - pending_count
        local progress_pct = total_targets > 0 and (completed_targets / total_targets) or 0
        
        set_color(0.92, 0.72, 0.22, 1)
        love.graphics.rectangle("fill", bar_x, bar_y, bar_w * progress_pct, bar_h, 3)
        
        set_color(0.5, 0.5, 0.6, 0.8)
        love.graphics.setFont(font_tiny)
        local status_msg = string.format("Database Synchronization: %d / %d Targets Resolved", completed_targets, total_targets)
        love.graphics.printf(status_msg, 0, bar_y + 18, w, "center")
        love.graphics.printf("Observer Site Profile: " .. loc_status, 0, bar_y + 36, w, "center")
        
        love.graphics.pop()
        return
    end

    -- --- DYNAMIC ORIENTATION LAYOUT CALCULATION GENERATOR ---
    local is_portrait = h >= w
    
    local list_w, bottom_h, map_w, map_h, map_left, map_top
    local header_h = 70 
    local tab_h = 40

    if not is_portrait then
        -- Standard horizontal split view layout for PC and horizontal tablets
        list_w = math.max(240, math.floor(w * 0.25))
        bottom_h = math.max(150, math.floor(h * 0.22))
        map_w = w - list_w
        map_h = h - bottom_h
        map_left = list_w
        map_top = 0
    else
        -- Stacked vertical viewport layout optimized directly for standard smartphone viewports
        list_w = w
        map_w = w
        map_h = math.floor(h * 0.38)
        bottom_h = math.max(160, math.floor(h * 0.24))
        map_left = 0
        map_top = 0
    end

    local each_tab_w = math.floor((list_w - 10) / 3)

    love.graphics.clear(0.01, 0.01, 0.03)

    -- --- VIEWPORT RENDERING SEGMENT: SIDEBAR MODULE LIST & INPUTS ---
    love.graphics.push("all")
    if is_portrait then
        -- Translate the layout panels down past the sky map boundaries
        love.graphics.translate(0, map_h)
    end
    
    set_color(0.05, 0.05, 0.1, 1)
    love.graphics.rectangle("fill", 0, 0, list_w, is_portrait and (h - map_h - bottom_h) or h)
    
    set_color(1, 1, 1, 1)
    love.graphics.setFont(font_bold)
    love.graphics.print("Observer Dashboard", 20, 20)
    
    set_color(0.4, 0.4, 0.6, 1)
    love.graphics.setFont(font_tiny)
    love.graphics.print(loc_status, 20, 42)

    love.graphics.setColor(0.05, 0.05, 0.1, 1) 
    love.graphics.rectangle("fill", 0, header_h, list_w, tab_h)

    if app_mode == "log" then love.graphics.setColor(0.92, 0.72, 0.22, 1) 
    else love.graphics.setColor(0.1, 0.15, 0.25, 1) end
    love.graphics.rectangle("fill", 5, header_h + 5, each_tab_w - 4, tab_h - 10, 4)

    if app_mode == "specs" then love.graphics.setColor(0.92, 0.72, 0.22, 1) 
    else love.graphics.setColor(0.1, 0.15, 0.25, 1) end
    love.graphics.rectangle("fill", 5 + each_tab_w, header_h + 5, each_tab_w - 4, tab_h - 10, 4)

    if app_mode == "journal" then love.graphics.setColor(0.92, 0.72, 0.22, 1) 
    else love.graphics.setColor(0.1, 0.15, 0.25, 1) end
    love.graphics.rectangle("fill", 5 + (each_tab_w * 2), header_h + 5, each_tab_w - 4, tab_h - 10, 4)

    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.setFont(font_tiny)
    love.graphics.printf("LOG", 5, header_h + 14, each_tab_w - 4, "center")
    love.graphics.printf("SPECS", 5 + each_tab_w, header_h + 14, each_tab_w - 4, "center")
    love.graphics.printf("JOURNAL", 5 + (each_tab_w * 2), header_h + 14, each_tab_w - 4, "center")
    
    local start_y = header_h + tab_h + 10
    local view_h = (is_portrait and (h - map_h - bottom_h) or (h - bottom_h)) - start_y

    if app_mode == "log" then
        local spacing = 65
        love.graphics.setScissor(0, is_portrait and (map_h + start_y) or start_y, list_w, view_h)
        love.graphics.push()
        love.graphics.translate(0, -scroll_y)

        for i, p_info in ipairs(Backend.planets) do
            local data = planets_data[p_info.name]
            local y = start_y + (i-1) * spacing 
            
            if selected_planet == p_info.name then
                set_color(1, 1, 1, 0.1)
                love.graphics.rectangle("fill", 5, y-5, list_w-10, 60, 5)
            end
            
            set_color(p_info.color[1], p_info.color[2], p_info.color[3], 1)
            love.graphics.setFont(font_bold)
            love.graphics.print(p_info.name, 20, y)
            
            if data then
                love.graphics.setFont(font_tiny)
                set_color(0.8, 0.8, 0.8, 1)
                love.graphics.print(string.format("ALT: %.1f°  AZ: %.1f° (%s)", data.alt, data.az, data.cardinal), 20, y + 22)
                set_color(0.5, 0.5, 0.7, 1)
                love.graphics.print(string.format("↑ %s  ↓ %s", data.rise or "--:--", data.set or "--:--"), 20, y + 38)
                
                if data.visible then set_color(0.2, 1, 0.2, 1) else set_color(0.4, 0.1, 0.1, 1) end
                love.graphics.circle("fill", list_w - 20, y + 10, 4)
            end
        end
        max_scroll = math.max(0, (#Backend.planets * spacing) - view_h + 20)
        love.graphics.pop()
        love.graphics.setScissor()
        
    elseif app_mode == "specs" then
        set_color(1, 1, 1, 1)
        love.graphics.setFont(font_bold)
        love.graphics.print("Equipment Profiles", 20, start_y)
        
        love.graphics.setFont(font_small)
        local box_w = math.min(100, list_w - 150)
        local box_left = list_w - box_w - 15
        
        set_color(0.8, 0.8, 0.8, 1)
        love.graphics.print("Scope FL (mm):", 20, start_y + 35)
        if active_input == "scope" then set_color(0.2, 0.4, 0.8, 1) else set_color(0.15, 0.15, 0.25, 1) end
        love.graphics.rectangle("fill", box_left, start_y + 30, box_w, 24, 4)
        set_color(1, 1, 1, 1)
        love.graphics.print(SCOPE_FL .. (active_input == "scope" and "|" or ""), box_left + 8, start_y + 34)

        set_color(0.8, 0.8, 0.8, 1)
        love.graphics.print("Eyepiece FL (mm):", 20, start_y + 75)
        if active_input == "eyepiece" then set_color(0.2, 0.4, 0.8, 1) else set_color(0.15, 0.15, 0.25, 1) end
        love.graphics.rectangle("fill", box_left, start_y + 70, box_w, 24, 4)
        set_color(1, 1, 1, 1)
        love.graphics.print(EYEPIECE_FL .. (active_input == "eyepiece" and "|" or ""), box_left + 8, start_y + 74)

        set_color(0.8, 0.8, 0.8, 1)
        love.graphics.print("Eyepiece AFOV (°):", 20, start_y + 115)
        if active_input == "afov" then set_color(0.2, 0.4, 0.8, 1) else set_color(0.15, 0.15, 0.25, 1) end
        love.graphics.rectangle("fill", box_left, start_y + 110, box_w, 24, 4)
        set_color(1, 1, 1, 1)
        love.graphics.print(EYEPIECE_AFOV .. (active_input == "afov" and "|" or ""), box_left + 8, start_y + 114)

        local calculated_mag = EYEPIECE_FL > 0 and (SCOPE_FL / EYEPIECE_FL) or 0
        set_color(0.6, 0.6, 0.8, 1)
        love.graphics.print("Resulting Mag: " .. string.format("%.1fx", calculated_mag), 20, start_y + 150)
        
        set_color(1, 1, 1, 1)
        love.graphics.setFont(font_bold)
        love.graphics.print("Observation Coordinates", 20, start_y + 185)
        
        love.graphics.setFont(font_tiny)
        local input_long_w = list_w - 40
        
        set_color(0.8, 0.8, 0.8, 1)
        love.graphics.print("Address/Street Name:", 20, start_y + 215)
        if active_input == "loc_address" then set_color(0.2, 0.4, 0.8, 1) else set_color(0.15, 0.15, 0.25, 1) end
        love.graphics.rectangle("fill", 20, start_y + 230, input_long_w, 20, 4)
        set_color(1, 1, 1, 1)
        love.graphics.print(loc_address .. (active_input == "loc_address" and "|" or ""), 26, start_y + 234)

        set_color(0.8, 0.8, 0.8, 1)
        love.graphics.print("City Name:", 20, start_y + 260)
        if active_input == "loc_city" then set_color(0.2, 0.4, 0.8, 1) else set_color(0.15, 0.15, 0.25, 1) end
        love.graphics.rectangle("fill", 20, start_y + 275, input_long_w, 20, 4)
        set_color(1, 1, 1, 1)
        love.graphics.print(loc_city .. (active_input == "loc_city" and "|" or ""), 26, start_y + 279)

        set_color(0.8, 0.8, 0.8, 1)
        love.graphics.print("Country Name:", 20, start_y + 305)
        if active_input == "loc_country" then set_color(0.2, 0.4, 0.8, 1) else set_color(0.15, 0.15, 0.25, 1) end
        love.graphics.rectangle("fill", 20, start_y + 320, input_long_w, 20, 4)
        set_color(1, 1, 1, 1)
        love.graphics.print(loc_country .. (active_input == "loc_country" and "|" or ""), 26, start_y + 324)

        set_color(0.2, 0.5, 0.7, 1)
        love.graphics.rectangle("fill", 20, start_y + 355, input_long_w, 25, 4)
        set_color(1, 1, 1, 1)
        love.graphics.setFont(font_small)
        love.graphics.printf("GEOCODE & UPDATE PANEL", 20, start_y + 360, input_long_w, "center")
        
    elseif app_mode == "journal" then
        set_color(1, 1, 1, 1)
        love.graphics.setFont(font_bold)
        love.graphics.print("Session Log Entries", 20, start_y)

        local journal_spacing = 45
        love.graphics.setScissor(0, is_portrait and (map_h + start_y + 25) or (start_y + 25), list_w, view_h - 30)
        love.graphics.push()
        love.graphics.translate(0, -journal_scroll_y)

        if #saved_logs == 0 then
            set_color(0.5, 0.5, 0.5, 1)
            love.graphics.setFont(font_tiny)
            love.graphics.printf("No recorded observation notes found in this session roster yet.", 20, start_y + 40, list_w - 40, "left")
        else
            for i, entry_text in ipairs(saved_logs) do
                local entry_y = start_y + 35 + (i - 1) * journal_spacing
                set_color(0.1, 0.1, 0.18, 0.6)
                love.graphics.rectangle("fill", 10, entry_y - 4, list_w - 20, journal_spacing - 8, 4)
                
                set_color(0.9, 0.9, 0.9, 1)
                love.graphics.setFont(font_tiny)
                love.graphics.printf(entry_text, 16, entry_y, list_w - 32, "left")
            end
        end

        journal_max_scroll = math.max(0, (#saved_logs * journal_spacing) - view_h + 40)
        love.graphics.pop()
        love.graphics.setScissor()
    end
    love.graphics.pop()

    -- --- VIEWPORT RENDERING SEGMENT: CIRCULAR CELESTIAL PLANISPHERE SKY MAP ---
    love.graphics.push("all")
    if not is_portrait then
        love.graphics.translate(map_left, map_top)
    end
    
    local map_cX = map_w / 2
    local map_cY = map_h / 2
    local map_R = math.min(map_w, map_h) * 0.42
    
    love.graphics.setLineWidth(1)
    for a = 0, 90, 30 do
        local r = map_R * (1 - (a / 90))
        set_color(1, 1, 1, a == 0 and 0.3 or 0.1)
        love.graphics.circle("line", map_cX, map_cY, r)
        if a < 90 and map_R > 100 then
            set_color(0.5, 0.5, 0.5, 0.5)
            love.graphics.setFont(font_tiny)
            love.graphics.print(a .. "°", map_cX + 5, map_cY - r - 12)
        end
    end

    local directions = {[0]="N", [45]="NE", [90]="E", [135]="SE", [180]="S", [225]="SW", [270]="W", [315]="NW"}
    for angle_deg = 0, 315, 45 do
        local angle_rad = math.rad(angle_deg - 90)
        local cos_a, sin_a = math.cos(angle_rad), math.sin(angle_rad)
        set_color(1, 1, 1, 0.05)
        love.graphics.line(map_cX, map_cY, map_cX + cos_a * map_R, map_cY + sin_a * map_R)
        
        if map_R > 120 then
            set_color(0.8, 0.8, 0.8, 0.8)
            love.graphics.setFont(font_tiny)
            love.graphics.printf((directions[angle_deg] or "") .. "\n" .. angle_deg .. "°", map_cX + cos_a * (map_R + 18) - 20, map_cY + sin_a * (map_R + 18) - 10, 40, "center")
        end
    end

    for _, p_info in ipairs(Backend.planets) do
        local data = planets_data[p_info.name]
        if data and data.visible then
            local x, y = get_map_coords(data.alt, data.az, map_cX, map_cY, map_R)
            if x and y then
                if selected_planet == p_info.name then
                    set_color(1, 0, 0, 0.4)
                    love.graphics.circle("line", x, y, 6)
                    love.graphics.circle("line", x, y, 14)
                end
                set_color(p_info.color[1], p_info.color[2], p_info.color[3], 1)
                love.graphics.circle("fill", x, y, 4)
                if map_R > 100 then
                    love.graphics.setFont(font_tiny)
                    love.graphics.print(p_info.name, x + 8, y - 8)
                end
            end
        end
    end
    
    -- --- TOUCH BUTTON HOTKEY CONTROL PANEL LAYOUT OVERLAY (FOR KEYBOARD-LESS MOBILE SCREEN DIALS) ---
    local btn_size = 32
    local pad = 12
    set_color(0.12, 0.12, 0.22, 0.8)
    
    -- Bottom Right Control Group Context box
    love.graphics.rectangle("fill", map_w - (btn_size * 4) - (pad * 5), map_h - btn_size - (pad * 2), (btn_size * 4) + (pad * 5), btn_size + (pad * 2), 6)
    set_color(1, 1, 1, 0.7)
    love.graphics.setFont(font_small)
    
    -- Draw individual virtual simulation command touch targets bounds
    local bx1 = map_w - (btn_size * 4) - (pad * 4)
    local by1 = map_h - btn_size - pad
    love.graphics.printf("T-", bx1, by1 + 8, btn_size, "center")
    love.graphics.printf("T+", bx1 + btn_size + pad, by1 + 8, btn_size, "center")
    love.graphics.printf("Z-", bx1 + (btn_size * 2) + (pad * 2), by1 + 8, btn_size, "center")
    love.graphics.printf("Z+", bx1 + (btn_size * 3) + (pad * 3), by1 + 8, btn_size, "center")
    
    love.graphics.pop()

    -- --- VIEWPORT RENDERING SEGMENT: BOTTOM RESPONSIVE CONSOLE BAR PANEL ---
    love.graphics.push("all")
    if is_portrait then
        love.graphics.translate(0, h - bottom_h)
    else
        love.graphics.translate(list_w, h - bottom_h)
    end
    
    local c_bar_w = is_portrait and w or (w - list_w)
    
    set_color(0.02, 0.02, 0.05, 1)
    love.graphics.rectangle("fill", 0, 0, c_bar_w, bottom_h)
    set_color(1, 1, 1, 0.15)
    love.graphics.line(0, 0, c_bar_w, 0)
    
    local sun_alt = planets_data["Sun"] and planets_data["Sun"].alt
    local sky_cond = Backend.getSkyCondition(sun_alt)
    
    set_color(1, 0.8, 0, 1)
    love.graphics.setFont(font_bold)
    love.graphics.print("UTC: " .. os.date("%H:%M", os.time() + time_offset) .. " | SKY: " .. sky_cond, 20, 15)

    local box_x = 20
    local box_w = math.max(180, math.floor(c_bar_w * 0.40))
    
    set_color(0.6, 0.6, 0.6, 0.8)
    love.graphics.setFont(font_tiny)
    love.graphics.print("Observation Notes Logger (Touch box field block):", box_x, 45)
    
    if active_input == "session_notes" then set_color(0.15, 0.25, 0.45, 1) else set_color(0.1, 0.1, 0.15, 1) end
    love.graphics.rectangle("fill", box_x, 65, box_w, 40, 4)
    set_color(1, 1, 1, 1)
    love.graphics.printf(log_notes .. (active_input == "session_notes" and "|" or ""), box_x + 8, 70, box_w - 16, "left")

    set_color(0.2, 0.5, 0.8, 1)
    love.graphics.rectangle("fill", box_x, 112, 130, 22, 4)
    set_color(1, 1, 1, 1)
    love.graphics.printf("COMMIT NOTE", box_x, 116, 130, "center")

    set_color(0.5, 0.5, 0.5, 1)
    love.graphics.print("T-/T+: Time Shifts  Z-/Z+: Zoom Swaps", 20, 32)

    -- Eyepiece simulation frame context rendering
    if selected_planet and planets_data[selected_planet] then
        local d = planets_data[selected_planet]
        local r = math.min(65, math.floor(bottom_h * 0.36))
        local tx = c_bar_w - r - 30
        local ty = bottom_h / 2 - 5
        
        local current_mag = EYEPIECE_FL > 0 and (SCOPE_FL / EYEPIECE_FL) or 1
        local current_tfov = (EYEPIECE_AFOV / current_mag) * 3600

        set_color(0.02, 0.02, 0.05, 1)
        love.graphics.circle("fill", tx, ty, r)
        set_color(1, 1, 1, 0.2)
        love.graphics.circle("line", tx, ty, r)

        love.graphics.stencil(function()
            love.graphics.circle("fill", tx, ty, r)
        end, "replace", 1)
        love.graphics.setStencilTest("equal", 1)

        love.graphics.push()
        love.graphics.translate(tx, ty)
        
        local field_rotation = math.rad(d.az or 0)
        love.graphics.rotate(math.pi + field_rotation) 
        
        local ang_size = d.ang_size or 5
        local px_size = (ang_size / current_tfov) * (r * 2)
        
        local p_color = {1, 1, 1}
        local p_type = ""
        for _, info in ipairs(Backend.planets) do 
            if info.name == selected_planet then 
                p_color = info.color 
                p_type = info.type
                break
            end 
        end

        if p_type == "star" then
            set_color(p_color[1], p_color[2], p_color[3], 1)
            love.graphics.circle("fill", 0, 0, 1.5)
        elseif p_type == "dso" then
            set_color(p_color[1], p_color[2], p_color[3], 0.3)
            love.graphics.circle("fill", 0, 0, 15)
            set_color(1, 1, 1, 0.7)
            love.graphics.points(0, 0)
        elseif selected_planet == "Saturn" then
            set_color(p_color[1], p_color[2], p_color[3], 0.6)
            love.graphics.ellipse("line", 0, 0, px_size * 1.2, px_size * 0.4)
            set_color(p_color[1], p_color[2], p_color[3], 1)
            love.graphics.circle("fill", 0, 0, px_size / 2)
        elseif selected_planet == "Jupiter" then
            set_color(p_color[1], p_color[2], p_color[3], 1)
            love.graphics.circle("fill", 0, 0, px_size / 2)
            set_color(1, 1, 1, 0.8)
            local moon_offsets = {-2.5, -1.8, 1.5, 3.2} 
            for _, offset in ipairs(moon_offsets) do 
                love.graphics.circle("fill", offset * px_size, 0, 1.5) 
            end
        else
            set_color(p_color[1], p_color[2], p_color[3], 1)
            love.graphics.circle("fill", 0, 0, px_size / 2)
        end
        love.graphics.pop()
        love.graphics.setStencilTest()

        set_color(1, 1, 1, 1)
        love.graphics.setFont(font_tiny)
        love.graphics.printf(selected_planet .. " (" .. string.format("%.0fx", current_mag) .. "x)", tx - 80, ty + r + 4, 160, "center")
    end
    love.graphics.pop()

    -- Solar hazard alert box placement geometry checks
    if selected_planet == "Sun" then
        love.graphics.push("all")
        set_color(1, 0, 0, 1)
        love.graphics.setFont(font_bold)
        love.graphics.printf("!!! SOLAR WARNING !!!\nFILTER REQUIRED", w - 210, is_portrait and (map_h - 60) or (h - bottom_h - 60), 200, "center")
        love.graphics.pop()
    end

    if is_dimmed then
        love.graphics.push("all")
        love.graphics.setColor(0, 0, 0, 0.85) 
        love.graphics.rectangle("fill", 0, 0, w, h)
        love.graphics.setColor(0.5, 0, 0, 1)
        love.graphics.setFont(font_small)
        love.graphics.printf("IDLE POWER OVERLAY: Night-Vision Lock Active\nTouch panel or interact to unlock panel frame context.", 0, h/2 - 20, w, "center")
        love.graphics.pop()
    end
end

function love.textinput(t)
    idle_time = 0
    is_dimmed = false
    if not active_input then return end
    
    if active_input == "scope" or active_input == "eyepiece" or active_input == "afov" then
        if t:match("%d") then 
            if active_input == "scope" then SCOPE_FL = tonumber(SCOPE_FL .. t) or SCOPE_FL
            elseif active_input == "eyepiece" then EYEPIECE_FL = tonumber(EYEPIECE_FL .. t) or EYEPIECE_FL
            elseif active_input == "afov" then EYEPIECE_AFOV = tonumber(EYEPIECE_AFOV .. t) or EYEPIECE_AFOV end
        end
    elseif active_input == "custom_name" then
        custom_name = custom_name .. t
    elseif active_input == "custom_id" then
        custom_id = custom_id .. t
    elseif active_input == "session_notes" then
        log_notes = log_notes .. t
    elseif active_input == "loc_address" then
        loc_address = loc_address .. t
    elseif active_input == "loc_city" then
        loc_city = loc_city .. t
    elseif active_input == "loc_country" then
        loc_country = loc_country .. t
    end
end

function love.keypressed(k)
    idle_time = 0
    is_dimmed = false

    if active_input then
        if k == "backspace" then
            if active_input == "scope" then
                local str = tostring(SCOPE_FL)
                SCOPE_FL = tonumber(str:sub(1, #str - 1)) or 0
            elseif active_input == "eyepiece" then
                local str = tostring(EYEPIECE_FL)
                EYEPIECE_FL = tonumber(str:sub(1, #str - 1)) or 0
            elseif active_input == "afov" then
                local str = tostring(EYEPIECE_AFOV)
                EYEPIECE_AFOV = tonumber(str:sub(1, #str - 1)) or 0
            elseif active_input == "custom_name" then
                custom_name = utf8_pop(custom_name)
            elseif active_input == "custom_id" then
                custom_id = utf8_pop(custom_id)
            elseif active_input == "session_notes" then
                log_notes = utf8_pop(log_notes)
            elseif active_input == "loc_address" then
                loc_address = utf8_pop(loc_address)
            elseif active_input == "loc_city" then
                loc_city = utf8_pop(loc_city)
            elseif active_input == "loc_country" then
                loc_country = utf8_pop(loc_country)
            end
        elseif k == "return" or k == "kpenter" or k == "escape" then
            active_input = nil
            love.keyboard.setTextInput(false) -- Safely drop mobile screen keys
        end
        return 
    end

    if k == "n" then 
        night_mode = not night_mode
    elseif k == "r" then 
        time_offset = 0 
        refresh_data()
    end
end

function love.mousepressed(x, y, button)
    if is_initializing then return end
    idle_time = 0
    is_dimmed = false
    
    local w, h = love.graphics.getDimensions()
    local is_portrait = h >= w
    
    local list_w, bottom_h, map_w, map_h, sidebar_y
    if not is_portrait then
        list_w = math.max(240, math.floor(w * 0.25))
        bottom_h = math.max(150, math.floor(h * 0.22))
        map_w = w - list_w
        map_h = h - bottom_h
        sidebar_y = 0
    else
        list_w = w
        map_w = w
        map_h = math.floor(h * 0.38)
        bottom_h = math.max(160, math.floor(h * 0.24))
        sidebar_y = map_h
    end
    
    -- Check for Virtual Hotkey clicks overlay matrix inside map coordinate space boundaries
    if x >= (map_left or list_w) and y <= map_h then
        local rx = x - (map_left or list_w)
        local btn_size = 32
        local pad = 12
        local bx1 = map_w - (btn_size * 4) - (pad * 4)
        local by1 = map_h - btn_size - pad
        
        if y >= by1 and y <= by1 + btn_size then
            if rx >= bx1 and rx <= bx1 + btn_size then
                time_offset = time_offset - 3600 refresh_data() return
            elseif rx >= bx1 + btn_size + pad and rx <= bx1 + (btn_size * 2) + pad then
                time_offset = time_offset + 3600 refresh_data() return
            elseif rx >= bx1 + (btn_size * 2) + (pad * 2) and rx <= bx1 + (btn_size * 3) + (pad * 2) then
                EYEPIECE_FL = math.max(1, EYEPIECE_FL - 1) return
            elseif rx >= bx1 + (btn_size * 3) + (pad * 3) and rx <= map_w - pad then
                EYEPIECE_FL = EYEPIECE_FL + 1 return
            end
        end
    end
    
    local header_h, tab_h = 70, 40
    local each_tab_w = math.floor((list_w - 10) / 3)
    local start_y = header_h + tab_h + 10
    local spacing = 65

    if button == 1 then
        -- Translate interaction points matching layout constraints
        local sy = is_portrait and (y - sidebar_y) or y
        local sx = x
        
        if sy > header_h and sy < header_h + tab_h and sx < list_w then
            active_input = nil
            love.keyboard.setTextInput(false)
            if sx > 0 and sx < 5 + each_tab_w then app_mode = "log"
            elseif sx >= 5 + each_tab_w and sx < 5 + (each_tab_w * 2) then app_mode = "specs"
            elseif sx >= 5 + (each_tab_w * 2) and sx < list_w then app_mode = "journal" end
        end

        if app_mode == "log" and sx < list_w and sy > start_y and y < (h - bottom_h) then
            local adjusted_y = sy + scroll_y
            local index = math.floor((adjusted_y - start_y) / spacing) + 1
            if Backend.planets[index] then 
                selected_planet = Backend.planets[index].name 
            end
        elseif app_mode == "specs" and sx < list_w then
            local box_w = math.min(100, list_w - 150)
            local box_left = list_w - box_w - 15
            
            if sx >= box_left and sx <= box_left + box_w then
                if sy >= start_y + 30 and sy <= start_y + 54 then active_input = "scope" love.keyboard.setTextInput(true)
                elseif sy >= start_y + 70 and sy <= start_y + 94 then active_input = "eyepiece" love.keyboard.setTextInput(true)
                elseif sy >= start_y + 110 and sy <= start_y + 134 then active_input = "afov" love.keyboard.setTextInput(true)
                else active_input = nil love.keyboard.setTextInput(false) end
            elseif sx >= 20 and sx <= list_w - 20 then
                if sy >= start_y + 230 and sy <= start_y + 250 then active_input = "loc_address" love.keyboard.setTextInput(true)
                elseif sy >= start_y + 275 and sy <= start_y + 295 then active_input = "loc_city" love.keyboard.setTextInput(true)
                elseif sy >= start_y + 320 and sy <= start_y + 340 then active_input = "loc_country" love.keyboard.setTextInput(true)
                elseif sy >= start_y + 355 and sy <= start_y + 380 then
                    active_input = nil
                    love.keyboard.setTextInput(false)
                    if loc_city ~= "" or loc_country ~= "" then geocode_address() end
                else active_input = nil love.keyboard.setTextInput(false) end
            else
                active_input = nil love.keyboard.setTextInput(false)
            end
        end

        -- Interaction coordinate calculations for the bottom bar logger elements
        local by = is_portrait and (y - (h - bottom_h)) or (y - (h - bottom_h))
        local bx = is_portrait and x or (x - list_w)
        local c_bar_w = is_portrait and w or (w - list_w)
        
        local log_box_x = 20
        local log_box_w = math.max(180, math.floor(c_bar_w * 0.40))
        
        if y >= (h - bottom_h) then
            if bx >= log_box_x and bx <= log_box_x + log_box_w and by >= 65 and by <= 105 then
                active_input = "session_notes"
                love.keyboard.setTextInput(true)
            elseif bx >= log_box_x and bx <= log_box_x + 130 and by >= 112 and by <= 134 then
                if log_notes ~= "" then
                    local formatted_entry = os.date("%H:%M") .. " [" .. selected_planet .. "] " .. log_notes
                    table.insert(saved_logs, formatted_entry)
                    love.filesystem.append("observer_journal.txt", formatted_entry .. "\n")
                    log_notes = ""
                    active_input = nil
                    love.keyboard.setTextInput(false)
                end
            else
                if active_input == "session_notes" then
                    active_input = nil
                    love.keyboard.setTextInput(false)
                end
            end
        end
    end
end