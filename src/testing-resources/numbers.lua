#!/usr/bin/env nexuslua

function IsPrime(parameters)
    local number = tonumber(parameters.number)
    q=math.sqrt(number)
    found=true
    for k=3,q,2 do
        d = number/k
        di = math.floor(d)
        if d==di then
            found=false
            break
        end
    end

    return {isPrime=tostring(found)}
end

addmessage("IsPrime")
