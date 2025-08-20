#!/usr/bin/env nexuslua

number_of_requests = 0
number_of_processed_requests = 0
count = 0

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

function CountPrime(parameters)
    -- printtable(parameters)
    number_of_processed_requests = number_of_processed_requests + 1

    if parameters.isPrime=="true" then
        count = count + 1
    end
    
    if (number_of_processed_requests == number_of_requests) then
          print("Found ", count, " prime numbers")
    end
end

function RequestPrimes(parameters)
    local n1 = 3 
    local n2 = 500001

    print("Checking prime numbers between ", n1, " and ", n2)

    for i=n1,n2,2 do
--        print("this agent should become idle")
        send("main", "IsPrime", {number=tostring(i), threads="24", queue="24", reply_to={message="CountPrime"}})
        number_of_requests = number_of_requests + 1
    end
end

if not isreplicated() then
    addmessage("IsPrime")
    addmessage("CountPrime")
    addmessage("RequestPrimes")

    send("main", "RequestPrimes", {})
    
    print("Hardware cores = " .. cores())
end
