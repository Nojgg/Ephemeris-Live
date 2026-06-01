package.path  = package.path .. ";C:/msys64/home/odin7/.luarocks/share/lua/5.4/?.lua;C:/msys64/home/odin7/.luarocks/share/lua/5.4/?/init.lua"
package.cpath = package.cpath .. ";C:/msys64/home/odin7/.luarocks/lib/lua/5.4/?.dll"

local http = require "socket.http"
local ltn12 = require "ltn12"
local math = require "math"

local planets = {
    { name = "Mercury", id = "199" },
    { name = "Venus",   id = "299" },
    { name = "Mars",    id = "499" },
    { name = "Jupiter", id = "599" },
    { name = "Saturn",  id = "699" },
    { name = "Uranus",  id = "799" },
    { name = "Neptune", id = "899" },
    { name = "Pluto",   id = "999" },
}

local center = "500@399" 
local step_size = "1 m"

local observer_lat = math.rad(49.7005244)
local observer_lon = 0.2521451 

local month_map = {Jan=1, Feb=2, Mar=3, Apr=4, May=5, Jun=6,
                   Jul=7, Aug=8, Sep=9, Oct=10, Nov=11, Dec=12}

local function parse_horizons_time(timestr)
    local year, mon_str, day, hour, min = timestr:match("(%d+)%-(%a+)%-(%d+) (%d+):(%d+)")
    local month = month_map[mon_str]
    if not month then error("Unknown month: "..tostring(mon_str)) end
    return os.time{
        year=tonumber(year), month=month, day=tonumber(day),
        hour=tonumber(hour), min=tonumber(min), sec=0
    }
end

print("Select time option:\n1) Current time\n2) Fixed time")
io.write("Enter choice (1 or 2): ")
local choice = io.read()
local start_time, stop_time
local current_time = os.time()
if choice == "1" then
    start_time = os.date("%Y-%m-%d %H:%M:%S", current_time)
    stop_time  = os.date("%Y-%m-%d %H:%M:%S", current_time + 10*60)
elseif choice == "2" then
    io.write("Enter date and time (YYYY-MM-DD HH:MM): ")
    local input_time = io.read()
    if not input_time:match("%d%d%d%d%-%d%d%-%d%d %d%d:%d%d") then
        error("Invalid date format. Use YYYY-MM-DD HH:MM")
    end
    start_time = input_time..":00"
    local t = os.time{
        year = tonumber(input_time:sub(1,4)),
        month = tonumber(input_time:sub(6,7)),
        day = tonumber(input_time:sub(9,10)),
        hour = tonumber(input_time:sub(12,13)),
        min = tonumber(input_time:sub(15,16)),
        sec = 0
    }
    stop_time = os.date("%Y-%m-%d %H:%M:%S", t + 10*60)
    current_time = t
else
    error("Invalid choice")
end

local function urlencode(str)
    return (str:gsub("\n", "\r\n"):gsub("([^%w%-%.%_%~])", function(c)
        return string.format("%%%02X", string.byte(c))
    end))
end
local function q(v) return urlencode("'"..v.."'") end
local function build_url(command)
    local params = {
        "format=text",
        "COMMAND="..q(command),
        "OBJ_DATA="..q("NO"),
        "MAKE_EPHEM="..q("YES"),
        "EPHEM_TYPE="..q("OBSERVER"),
        "CENTER="..q(center),
        "START_TIME="..q(start_time),
        "STOP_TIME="..q(stop_time),
        "STEP_SIZE="..q(step_size),
        "QUANTITIES="..q("1"),
        "ANG_FORMAT="..q("DEG"),
        "EXTRA_PREC="..q("YES"),
    }
    return "https://ssd.jpl.nasa.gov/api/horizons.api?"..table.concat(params,"&")
end

local function fetch(url)
    local body = {}
    local _, code = http.request{
        url = url,
        sink = ltn12.sink.table(body),
        headers = { ["User-Agent"] = "LuaSocket" }
    }
    local text = table.concat(body)
    if tonumber(code) ~= 200 then error("HTTP request failed: "..code) end
    return text
end

local function parse_rows(text)
    local rows = {}
    local in_table = false
    for line in text:gmatch("[^\r\n]+") do
        if line:match("^%$%$SOE") then
            in_table = true
        elseif line:match("^%$%$EOE") then
            in_table = false
        elseif in_table and line:match("%S") then
            local tokens = {}
            for tok in line:gmatch("%S+") do tokens[#tokens+1] = tok end
            if #tokens >= 4 then
                local ra = tonumber(tokens[3])
                local dec = tonumber(tokens[4])
                if ra and dec then
                    rows[#rows+1] = {time=tokens[1].." "..tokens[2], ra=ra, dec=dec}
                end
            end
        end
    end
    return rows
end

local function lst_deg(time_utc)
    local jd = 2440587.5 + time_utc/86400
    local T = (jd - 2451545.0)/36525
    local gst = 280.46061837 + 360.98564736629*(jd-2451545.0) + 0.000387933*T^2 - T^3/38710000
    gst = gst % 360
    return (gst + observer_lon) % 360
end

local function ra_dec_to_altaz(ra_deg, dec_deg, time_utc)
    local ra = math.rad(ra_deg)
    local dec = math.rad(dec_deg)
    local lst = math.rad(lst_deg(time_utc))
    local ha = lst - ra
    local sin_alt = math.sin(dec)*math.sin(observer_lat) + math.cos(dec)*math.cos(observer_lat)*math.cos(ha)
    local alt = math.asin(sin_alt)
    local cos_az = (math.sin(dec) - math.sin(alt)*math.sin(observer_lat)) / (math.cos(alt)*math.cos(observer_lat))
    local az = math.acos(math.min(math.max(cos_az,-1),1))
    if math.sin(ha) > 0 then az = 2*math.pi - az end
    return math.deg(alt), math.deg(az)
end

local function rise_transit_set(ra_deg, dec_deg, date_utc)
    local lat = observer_lat
    local dec = math.rad(dec_deg)
    local h0 = math.rad(0)
    local cos_H = (math.sin(h0) - math.sin(lat)*math.sin(dec)) / (math.cos(lat)*math.cos(dec))
    if cos_H < -1 or cos_H > 1 then
        return "Never", "Never", "Never", 0
    end
    local ra = math.rad(ra_deg)
    local lst = math.rad(lst_deg(date_utc))
    local transit_time = date_utc + (ra - lst)/(2*math.pi)*86400
    local rise_time = transit_time - math.acos(math.min(math.max(cos_H,-1),1))/(2*math.pi)*86400
    local set_time = transit_time + math.acos(math.min(math.max(cos_H,-1),1))/(2*math.pi)*86400

    rise_time = math.floor(rise_time + 0.5)
    transit_time = math.floor(transit_time + 0.5)
    set_time = math.floor(set_time + 0.5)

    return os.date("%H:%M", rise_time), os.date("%H:%M", transit_time), os.date("%H:%M", set_time), transit_time
end

local function az_to_cardinal(az_deg)
    local directions = {"N","NE","E","SE","S","SW","W","NW"}
    local index = math.floor(((az_deg + 22.5) % 360) / 45) + 1
    return directions[index]
end


local function format_observation(alt, az, transit_sec, current_sec)
    local visible = alt > 0
    local near_transit = visible and math.abs(current_sec - transit_sec)/60 <= 5
    local cardinal = az_to_cardinal(az)
    local flag = visible and "YES" or "NO"
    if near_transit then flag = flag.."*" end
    if visible then
        local color_start = "\27[32m" 
        local color_end = "\27[0m"
        return color_start..flag.." ("..cardinal..", "..string.format("%.1f°", alt)..")"..color_end
    else
        return flag.." ("..cardinal..", "..string.format("%.1f°", alt)..")"
    end
end

local visible_planets = {}
for _, planet in ipairs(planets) do
    print("\n==== "..planet.name.." ====")
    local url = build_url(planet.id)
    local text = fetch(url)
    local rows = parse_rows(text)

    if #rows == 0 then
        print("No RA/DEC rows found.\n")
    else
        print(string.format("%-16s %-12s %-12s %-8s %-8s %-20s %-8s %-8s %-8s",
            "Time", "RA (deg)", "DEC (deg)", "ALT", "AZ", "Visible (Cardinal, Alt)", "Rise", "Transit", "Set"))
        print(string.rep("-", 120))

        for _, row in ipairs(rows) do
            local utc_time = parse_horizons_time(row.time)
            local alt, az = ra_dec_to_altaz(row.ra, row.dec, utc_time)
            local rise, transit_time_str, settime, transit_sec = rise_transit_set(row.ra, row.dec, utc_time)
            local visible_flag = format_observation(alt, az, transit_sec, utc_time)

            local line = string.format("%-16s %-12.6f %-12.6f %-8.2f %-8.2f %-20s %-8s %-8s %-8s",
                row.time, row.ra, row.dec, alt, az, visible_flag, rise, transit_time_str, settime)

            if math.abs(os.difftime(utc_time, current_time)) < 60 then
                local color_start = "\27[1;32m" -- bright green
                local color_end   = "\27[0m"
                line = color_start..line..color_end
            end

            print(line)

            if alt > 0 then
                visible_planets[#visible_planets+1] = {
                    name = planet.name, alt = alt, az = az,
                    rise = rise, transit = transit_time_str, settime = settime
                }
            end
        end
    end
end


print("\n=== Best Planets to Observe Right Now ===")
if #visible_planets == 0 then
    print("No planets currently visible from your location.")
else
    local best_per_planet = {}
    for _, p in ipairs(visible_planets) do
        if not best_per_planet[p.name] or p.alt > best_per_planet[p.name].alt then
            best_per_planet[p.name] = p
        end
    end

    local colors = {
        Mercury = "\27[35m", Venus = "\27[33m", Mars = "\27[31m", Jupiter = "\27[36m",
        Saturn = "\27[34m", Uranus = "\27[32m", Neptune = "\27[37m", Pluto = "\27[95m",
    }
    local reset = "\27[0m"

    local sorted = {}
    for _, p in pairs(best_per_planet) do table.insert(sorted, p) end
    table.sort(sorted, function(a,b) return a.alt > b.alt end)

    for _, p in ipairs(sorted) do
        local color = colors[p.name] or reset
        local cardinal = az_to_cardinal(p.az)
        print(string.format("%s%s: Alt %.1f°, Az %.1f° (%s)%s",
            color, p.name, p.alt, p.az, cardinal, reset))
    end
end