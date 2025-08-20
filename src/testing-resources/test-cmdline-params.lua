#!/usr/bin/env nexuslua

-- printtable(arg)

-- print("script name: " .. arg[tostring(0)])
print("script name: " .. arg[0])

local start_idx, end_idx = 1, 1
--while arg[tostring(-start_idx)] do start_idx = start_idx + 1 end
--while arg[tostring(end_idx)] do end_idx = end_idx + 1 end
while arg[-start_idx] do start_idx = start_idx + 1 end
while arg[end_idx] do end_idx = end_idx + 1 end

for i = -start_idx + 1, end_idx - 1 do
--  print("Argument " .. i .. " : " .. tostring(arg[tostring(i)]))
  print("Argument " .. i .. " : " .. tostring(arg[i]))
end
