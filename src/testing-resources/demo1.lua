#!/usr/bin/env nexuslua

local nCheckedPrimes = 0
local count = 0
local startTime = time()

function IsPrime(number)
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

    return found
end

function CountPrime(flag)
    nCheckedPrimes = nCheckedPrimes + 1

    if flag then
        count = count + 1
    end
end

local n1 = 10000000001 
local n2 = 10000100001

print("Checking prime numbers between ", n1, " and ", n2)

for i=n1,n2,2 do
    local result = IsPrime(i)
    CountPrime(result)
end

local endTime = time()
print("Checked ", nCheckedPrimes, " numbers in ", (endTime-startTime)/1.0e8, " seconds, found ", count, " prime")
