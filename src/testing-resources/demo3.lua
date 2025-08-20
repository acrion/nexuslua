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
    
    if (nCheckedPrimes == nRequests) then
    	local endTime = time()
        print("Checked ", nCheckedPrimes, " numbers in ", (endTime-startTime)/1.0e8, " seconds, found ", count, " prime")
    end
end

function RequestPrimes(parameters)
    print("Checking prime numbers between ", parameters.n1, " and ", parameters.n2)

    for i=parameters.n1,parameters.n2,2 do
        send("main", "IsPrime", {number=i, reply_to={message="CountPrime"}})
        nRequests = nRequests + 1
    end
end

addmessage("IsPrime")
addmessage("CountPrime")
addmessage("RequestPrimes")

send("main", "RequestPrimes", {n1=10000000001,
                               n2=10000100001})
