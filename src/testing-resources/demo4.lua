#!/usr/bin/env nexuslua

local nRequests = 0
local nCheckedPrimes = 0
local count = 0
local startTime = time()

function IsPrime(parameters)
    local q=math.sqrt(parameters.number)
    local found=true
    for k=3,q,2 do
        local d = parameters.number/k
        local di = math.floor(d)
        if d==di then
            found=false
            break
        end
    end

    return {isPrime=found}
end

function CountPrime(parameters)
    nCheckedPrimes = nCheckedPrimes + 1

    if parameters.isPrime then
        count = count + 1
    end
    
    --if (nCheckedPrimes == nRequests) then
    local endTime = time()
    local logstr = "Checked " .. tostring(nCheckedPrimes) .. " numbers in " .. tostring((endTime-startTime)/1.0e8) .. " seconds, found " .. tostring(count) .. " prime, requests: " .. tostring(nCheckedPrimes) .. "/" .. tostring(nRequests)
    print(logstr)
    log(logstr)
    --end
end

function RequestPrimes(parameters)
    local maxThreads = (cores()+1) // 2
    print("Checking prime numbers between ", parameters.n1, " and ", parameters.n2, " using " .. maxThreads .. " threads.")

    for i=parameters.n1,parameters.n2,2 do
        send("main", "IsPrime", {number=i, threads=maxThreads, reply_to={message="CountPrime", merge={unreplicated=true}}})
        nRequests = nRequests + 1
    end
end

if not isreplicated() then
    local config = getconfig()
    config.internal.logReplication = true
    setconfig(config)

    addmessage("IsPrime")
    addmessage("CountPrime")
    addmessage("RequestPrimes")

    send("main", "RequestPrimes", {n1=10000000001,
                                   n2=10000000051})
end
