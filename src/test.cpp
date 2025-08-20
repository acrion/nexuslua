/*
Copyright (c) 2025 acrion innovations GmbH
Authors: Stefan Zipproth, s.zipproth@acrion.ch

This file is part of nexuslua, see https://github.com/acrion/nexuslua and https://nexuslua.org

nexuslua is offered under a commercial and under the AGPL license.
For commercial licensing, contact us at https://acrion.ch/sales. For AGPL licensing, see below.

AGPL licensing:

nexuslua is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

nexuslua is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with nexuslua. If not, see <https://www.gnu.org/licenses/>.
*/

#ifdef GENERATED_WITH_DBG_CONFIGURATION

    #include "main.hpp" // for process_cmd_line

    #include <cbeam/filesystem/io.hpp>                // for write_file, create_unique_temp_dir
    #include <cbeam/filesystem/stdout_redirector.hpp> // for cbeam::filesystem::stdout_redirector
    #include <cbeam/lifecycle/singleton.hpp>          // for cbeam::lifecycle::singleton_control

    #include <gtest/gtest.h>

    #include <filesystem> // for std::filesystem related functions
    #include <map>        // for std::map
    #include <memory>     // for std::unique_ptr
    #include <mutex>      // for std::mutex, std::unique_lock
    #include <string>     // for std::string

std::map<std::string, std::pair<std::string, std::string>> lua_test_sources = {
    {"hello.lua", {R"(print("hello"))",
                   R"(hello
)"}},
    {"hello2.lua", {R"(print("hello2"))",
                    R"(hello2
)"}},
    // TODO primes.lua test performance suffers from logging, which is default in Debug config. Switching off logging does not work, needs to be fixed.
    {"primes.lua", {R"(
local nRequests = 0
local nCheckedPrimes = 0
local count = 0

function CountPrime(parameters)
    nCheckedPrimes = nCheckedPrimes + 1

    if parameters.isPrime then
        count = count + 1
    end

    if (nCheckedPrimes == nRequests) then
        print("Found " .. count .. " prime numbers")
    end
end

function RequestPrimes(parameters)
    local config = getconfig()
    config.internal.logReplication = true
    config.internal.logMessages = false
    setconfig(config)

    local maxThreads = (cores()+1) // 2

    for i = parameters.n1, parameters.n2, 2 do
        send("numbers", "IsPrime", { number = i, threads = maxThreads, reply_to = { agent = "main", message = "CountPrime" } })
        nRequests = nRequests + 1
    end
end

local config = getconfig()
config.internal.logReplication = true
config.internal.logMessages = false
setconfig(config)

if not isreplicated() then
    local numbersAgent = [==[
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

local config = getconfig()
config.internal.logReplication = true
config.internal.logMessages = false
setconfig(config)
]==]

    addagent("numbers", numbersAgent, {"IsPrime"})
    addmessage("CountPrime")
    addmessage("RequestPrimes")

    send("main", "RequestPrimes", {n1=1,
                                   n2=1001})
end
)",
                    R"(Found 168 prime numbers
)"}}

};

class nexusluaTest : public ::testing::Test
{
protected:
    std::unique_lock<std::mutex> lock;
    static inline std::mutex     test_mutex; ///< prevent parallel execution of tests
    std::filesystem::path        output_dir;
    std::filesystem::path        logFile;
    std::filesystem::path        original_current_path;

    void SetUp() override
    {
        lock                  = std::unique_lock<std::mutex>(test_mutex);
        original_current_path = std::filesystem::current_path();
        output_dir            = cbeam::filesystem::create_unique_temp_dir();

        std::filesystem::create_directories(output_dir);
        for (const auto& [filename, content] : lua_test_sources)
        {
            cbeam::filesystem::write_file(output_dir / filename, content.first);
        }

        logFile = output_dir / "lua.log";
        std::filesystem::current_path(output_dir);
    }

    void TearDown() override
    {
        lock.unlock();
        std::filesystem::current_path(original_current_path);

        if (std::filesystem::exists(output_dir))
        {
            std::filesystem::remove_all(output_dir);
        }
    }
};

TEST_F(nexusluaTest, lua_tests)
{
    for (const auto& [filename, test_data] : lua_test_sources)
    {
        const auto& [lua_code, expected_output] = test_data;

        std::string script_path = (output_dir / filename).string();
        char*       argv[]{(char*)test_info_->name(), (char*)script_path.c_str()};

        {
            cbeam::filesystem::stdout_redirector redirect(logFile);
            process_cmd_line(2, argv);
        }

        std::string actual_output = cbeam::filesystem::read_file(logFile);
        EXPECT_EQ(actual_output, expected_output);
    }
}
#endif // GENERATED_WITH_DBG_CONFIGURATION
