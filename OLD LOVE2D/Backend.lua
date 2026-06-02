Backend = {}

Backend.lat = 46.2276
Backend.lon = 2.2137

local OBSERVER_LAT = math.rad(Backend.lat)
local CENTER = "coord@399"
local STEP_SIZE = "1 m"

Backend.planets = {
    { name = "Sun",     id = "10",      color = {1.0, 0.9, 0.2},  type = "star" },
    { name = "Mercury", id = "199",     color = {0.80, 0.55, 0.85}, type = "planet" },
    { name = "Venus",   id = "299",     color = {0.95, 0.85, 0.35}, type = "planet" },
    { name = "Mars",    id = "499",     color = {0.90, 0.30, 0.25}, type = "planet" },
    { name = "Jupiter", id = "599",     color = {0.35, 0.85, 0.90}, type = "planet" },
    { name = "Saturn",  id = "699",     color = {0.40, 0.55, 0.90}, type = "planet" },
    { name = "Uranus",  id = "799",     color = {0.35, 0.90, 0.65}, type = "planet" },
    { name = "Neptune", id = "899",     color = {0.35, 0.55, 1.00}, type = "planet" },
    { name = "Pluto",   id = "999",     color = {0.80, 0.45, 1.00}, type = "planet" },
    { name = "Moon",    id = "301",     color = {0.95, 0.95, 0.95}, type = "satellite" },
}

local function apply_refraction(alt_deg)
    if alt_deg < -0.8 then return alt_deg end
    local r = 1.02 / math.tan(math.rad(alt_deg + 10.3 / (alt_deg + 5.11)))
    return alt_deg + (r / 60)
end

function Backend.getSkyCondition(sun_alt)
    if not sun_alt then return "Loading..." end
    if sun_alt > 0 then return "Jour"
    elseif sun_alt > -6 then return "Crép. Civil"
    elseif sun_alt > -12 then return "Crép. Nautique"
    elseif sun_alt > -18 then return "Crép. Astro"
    else return "Nuit Noire" end
end

function Backend.initLocation()
    local thread_code = [[
        local handle = io.popen('curl -s "http://ip-api.com/json/"')
        local result = handle:read("*a")
        handle:close()
        if result then
            local lat = result:match('"lat":([%d%.%-]+)')
            local lon = result:match('"lon":([%d%.%-]+)')
            love.thread.getChannel("loc_res"):push({lat = tonumber(lat), lon = tonumber(lon)})
        end
    ]]
    love.thread.newThread(thread_code):start()
end

function Backend.updateLocation()
    local res = love.thread.getChannel("loc_res"):pop()
    if res and res.lat and res.lon then
        Backend.lat = res.lat
        Backend.lon = res.lon
        OBSERVER_LAT = math.rad(Backend.lat) 
        return true
    end
    return false
end

local function urlencode(str)
    return (str:gsub("\n","\r\n"):gsub("([^%w%-%.%_%~])", function(c)
        return string.format("%%%02X", string.byte(c))
    end))
end

local function q(v) return urlencode("'"..v.."'") end

local function lst_deg(time_utc)
    local jd = 2440587.5 + time_utc/86400
    local T = (jd - 2451545.0)/36525
    local gst = 280.46061837 + 360.98564736629*(jd-2451545.0) + 0.000387933*T^2 - T^3/38710000
    gst = (gst % 360 + 360) % 360
    return (gst + Backend.lon) % 360
end

local function ra_dec_to_altaz(ra_deg, dec_deg, time_utc)
    local ra, dec, lst = math.rad(ra_deg), math.rad(dec_deg), math.rad(lst_deg(time_utc))
    local ha = lst - ra
    local sin_alt = math.sin(dec)*math.sin(OBSERVER_LAT) + math.cos(dec)*math.cos(OBSERVER_LAT)*math.cos(ha)
    local alt = math.asin(math.max(-1, math.min(1, sin_alt)))
    local cos_az = (math.sin(dec) - math.sin(alt)*math.sin(OBSERVER_LAT)) / (math.cos(alt)*math.cos(OBSERVER_LAT))
    local az = math.acos(math.max(-1, math.min(1, cos_az)))
    if math.sin(ha) > 0 then az = 2*math.pi - az end
 
    return apply_refraction(math.deg(alt)), math.deg(az)
end

local function rise_transit_set(ra_deg, dec_deg, date_utc)
    local dec = math.rad(dec_deg)
    local cos_H = (math.sin(0) - math.sin(OBSERVER_LAT)*math.sin(dec)) / (math.cos(OBSERVER_LAT)*math.cos(dec))
    if cos_H < -1 then return "∞", "∞", "∞" end 
    if cos_H > 1 then return "✗", "✗", "✗" end
    local ra, lst, H = math.rad(ra_deg), math.rad(lst_deg(date_utc)), math.acos(math.max(-1, math.min(1, cos_H)))
    local tr = date_utc + (ra - lst)/(2*math.pi)*86400
    return os.date("%H:%M", tr - H/(2*math.pi)*86400), os.date("%H:%M", tr), os.date("%H:%M", tr + H/(2*math.pi)*86400)
end

local function build_url(command, start_time, stop_time)
    local params = {
        "format=text",
        "COMMAND="..q(command),
        "OBJ_DATA="..q("NO"),
        "MAKE_EPHEM="..q("YES"),
        "EPHEM_TYPE="..q("OBSERVER"),
        "CENTER="..q(CENTER),
        "START_TIME="..q(start_time),
        "STOP_TIME="..q(stop_time),
        "STEP_SIZE="..q(STEP_SIZE),
        "QUANTITIES="..q("1,13"),
        "ANG_FORMAT="..q("DEG"),
        "EXTRA_PREC="..q("YES"),
    }
    return "https://ssd.jpl.nasa.gov/api/horizons.api?"..table.concat(params,"&")
end

local function parse_rows(text)
    local rows, in_table = {}, false
    for line in text:gmatch("[^\r\n]+") do
        if line:match("^%$%$SOE") then in_table = true
        elseif line:match("^%$%$EOE") then in_table = false
        elseif in_table and line:match("%S") then
            local tokens = {}
            for tok in line:gmatch("%S+") do tokens[#tokens+1] = tok end
            if #tokens >= 5 then
                local ra, dec, ang = tonumber(tokens[#tokens-2]), tonumber(tokens[#tokens-1]), tonumber(tokens[#tokens])
                if ra and dec then 
                    rows[#rows+1] = {ra=ra, dec=dec, ang_size=ang or 0.1} 
                end
            end
        end
    end
    return rows
end

function Backend.fetch_planet(planet, time_utc, callback)
    local start_t = os.date("!%Y-%m-%d %H:%M:%S", time_utc)
    local stop_t = os.date("!%Y-%m-%d %H:%M:%S", time_utc + 60)
    local url = build_url(planet.id, start_t, stop_t)

    local thread_code = [[
        local url, planet_name = ...
        local cmd = string.format('curl -s -L "%s"', url)
        local handle = io.popen(cmd)
        local result = handle:read("*a")
        handle:close()
        local ok = (result ~= nil and result:match("API VERSION"))
        love.thread.getChannel("planet_result"):push({
            planet = planet_name, body = result or "", ok = ok
        })
    ]]
    love.thread.newThread(thread_code):start(url, planet.name)
    Backend._pending = Backend._pending or {}
    Backend._pending[planet.name] = { callback = callback, time_utc = time_utc, planet = planet }
end

function Backend.poll()
    local msg = love.thread.getChannel("planet_result"):pop()
    if not msg then return end
    local entry = Backend._pending[msg.planet]
    if not entry then return end
    Backend._pending[msg.planet] = nil
    if not msg.ok then entry.callback(nil, "Net Err") return end
    
    local rows = parse_rows(msg.body)
    if #rows == 0 then entry.callback(nil, "Parse Err") return end
    local row = rows[1]
    
    local alt, az = ra_dec_to_altaz(row.ra, row.dec, entry.time_utc)
    local rise, tr_str, set = rise_transit_set(row.ra, row.dec, entry.time_utc)
    
    entry.callback({
        planet = entry.planet, 
        alt = alt, 
        az = az, 
        cardinal = ({"N","NE","E","SE","S","SW","W","NW"})[math.floor(((az+22.5)%360)/45)+1],
        rise = rise, 
        transit = tr_str, 
        set = set, 
        visible = alt > 0, 
        ang_size = row.ang_size
    }, nil)
end

return Backend