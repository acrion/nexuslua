#!/usr/bin/env nexuslua

n=0
last=1000000

print("Counting prime numbers...")

for i=3,last,2 do
    q=math.sqrt(i)
    found=true
    for k=3,q,2 do
        d = i/k
        di = math.floor(d)
        if d==di then
            found=false
            break
        end
    end

    if found then  
        n = n + 1
    end
end

print("Found " .. n .. " primes between 3 and " .. last)

