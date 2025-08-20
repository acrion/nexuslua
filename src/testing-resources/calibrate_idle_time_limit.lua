#!/usr/bin/env nexuslua
local nRequests = 0
local nCheckedPrimes = 0
local count = 0

function CountPrime(parameters)
    nCheckedPrimes = nCheckedPrimes + 1

    if parameters.isPrime then
        count = count + 1
    end

    if (nCheckedPrimes == nRequests) then
        print("Checked ", nCheckedPrimes, " numbers, found ", count, " prime")
    end
end

function RequestPrimes(parameters)
    local maxThreads = (cores()+1) // 2
    print("Checking prime numbers between ", parameters.n1, " and ", parameters.n2, " using " .. maxThreads .. " threads.")

    for i = parameters.n1, parameters.n2, 2 do
        send("numbers", "IsPrime", { number = i, threads = maxThreads, reply_to = { agent = "main", message = "CountPrime" } })
        nRequests = nRequests + 1
    end
end

if not isreplicated() then
    local numbersCode = [==[
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

    return {isPrime=found}
end

addmessage("IsPrime")
]==]

    local config = getconfig()
    -- printtable(config)
    config.internal.luaStartNewThreadTime = 0.01
    -- printtable(config)
    setconfig(config)

    addagent("numbers", numbersCode) -- alternatively, put above code into numbers.lua and write addagent("numbers", readfile("numbers.lua"))
    addmessage("CountPrime")
    addmessage("RequestPrimes")

    send("main", "RequestPrimes", { n1 = 100000000001,
                                    n2 = 100000100001 })
end
