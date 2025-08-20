#!/usr/bin/env nexuslua

local function isLeapYear(year)
    if year % 400 == 0 then
        return true
    elseif year % 100 == 0 then
        return false
    elseif year % 4 == 0 then
        return true
    else
        return false
    end
end

local SECONDS_PER_DAY = 60 * 60 * 24
local DAYS_IN_MONTH = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}

local function calculateDate(seconds_since_1970)
    local days_since_1970 = math.floor(seconds_since_1970 / SECONDS_PER_DAY)
    local year = 1970
    while days_since_1970 >= (isLeapYear(year) and 366 or 365) do
        days_since_1970 = days_since_1970 - (isLeapYear(year) and 366 or 365)
        year = year + 1
    end
    if isLeapYear(year) then
        DAYS_IN_MONTH[2] = 29
    else
        DAYS_IN_MONTH[2] = 28
    end
    local month = 1
    while days_since_1970 >= DAYS_IN_MONTH[month] do
        days_since_1970 = days_since_1970 - DAYS_IN_MONTH[month]
        month = month + 1
    end
    local day = days_since_1970 + 1

    local remainingSeconds = seconds_since_1970 % SECONDS_PER_DAY
    local hour = math.floor(remainingSeconds / 3600)
    remainingSeconds = remainingSeconds % 3600
    local minute = math.floor(remainingSeconds / 60)
    local second = remainingSeconds % 60

    return year, month, day, hour, minute, second
end

local seconds_since_1970 = time() * 0.00000001
year, month, day, hour, minutes, seconds = calculateDate(seconds_since_1970)
print(year, month, day, hour, minutes, seconds)
