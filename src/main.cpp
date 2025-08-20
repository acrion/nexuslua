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

#include "main.hpp"

#include <map>

#include <cbeam/container/find.hpp>

#include "nexuslua/agent.hpp"
#include "nexuslua/agents.hpp"
#include "nexuslua/description.hpp"

#include "nexuslua/lua_table.hpp"
#include "nexuslua/message.hpp"

#include <cbeam/convert/xpod.hpp>

#include <cbeam/container/find.hpp>
#include <cbeam/convert/nested_map.hpp>
#include <cbeam/convert/string.hpp>
#include <cbeam/logging/log_manager.hpp>

#include <condition_variable>
#include <mutex>
#include <stdexcept>

#ifdef GENERATED_WITH_DBG_CONFIGURATION
    #include <gtest/gtest.h>
#endif

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>

using namespace std::string_literals;

std::mutex              mtx;
std::condition_variable cv;
bool                    messageProcessed = false;
bool                    pluginSuccess    = false;
std::string             pluginMessage;
nexuslua::LuaTable      pluginResult;
const std::string       agentName   = "nexuslua_executable";
const std::string       messageName = "handle_result";

nexuslua::AgentMessage GetParameters(std::shared_ptr<nexuslua::agents> agent_group, int& i, char** argv)
{
    const std::string pluginName = argv[i++];
    const std::string toolName   = argv[i++];

    nexuslua::AgentMessage message = agent_group->GetMessage(pluginName, toolName);

    std::cout << "Configuring " << pluginName << " --> " << message.GetDisplayName() << std::endl;
    return message;
}

void help(std::shared_ptr<nexuslua::agents> agent_group, int& i, int argc, char** argv)
{
    if (i == argc)
    {
        for (const auto& it : agent_group->GetPlugins())
        {
            std::cout << it.second->GetName() << std::endl;

            for (const auto& it2 : it.second->GetMessages())
            {
                const nexuslua::AgentMessage& message = it2.second;
                std::cout << "\t" << message.GetMessageName() << "(";
                bool first = true;
                for (const auto& it3 : message.GetParameterDescriptions())
                {
                    if (!first)
                    {
                        std::cout << ", ";
                    }

                    auto typeStringIterator = it3.second.data.find("type");

                    if (typeStringIterator != it3.second.data.end())
                    {
                        const std::string typeString = cbeam::container::get_value_or_default<std::string>(typeStringIterator->second);
                        std::cout << typeString << " ";
                    }

                    using cbeam::container::xpod::operator<<;
                    std::cout << it3.first;
                    first = false;
                }
                std::cout << ")";
                if (!message.GetDescription().empty())
                {
                    std::cout << " -- " << message.GetDescription();
                }
                std::cout << std::endl;
            }
        }
    }
    else if (i <= argc - 2)
    {
        auto descriptions = GetParameters(agent_group, i, argv).GetParameterDescriptions();
        std::cout << cbeam::convert::to_string(descriptions);
    }
    else
    {
        throw std::runtime_error("For help of a specific plugin message, specify both the plugin and its message name.");
    }
}

void HandleResult(const std::shared_ptr<nexuslua::Message>& resultMessage)
{
    const auto& lastError = pluginResult.data.find("error");
    if (lastError != pluginResult.data.end())
    {
        pluginResult.data.erase(lastError);
    }
    const auto& lastMessage = pluginResult.data.find("message");
    if (lastMessage != pluginResult.data.end())
    {
        pluginResult.data.erase(lastMessage);
    }
    pluginResult.merge(resultMessage->parameters);
    auto error    = pluginResult.data.find("error");
    auto message  = pluginResult.data.find("message");
    pluginSuccess = error == pluginResult.data.end();
    if (pluginSuccess)
    {
        pluginMessage = message == pluginResult.data.end() ? "" : cbeam::container::get_value_or_default<std::string>(message->second);
    }
    else
    {
        pluginMessage = cbeam::container::get_value_or_default<std::string>(error->second);
    }
    {
        std::lock_guard<std::mutex> lock(mtx);
        messageProcessed = true;
    }

    cv.notify_one();
}

void run(std::shared_ptr<nexuslua::agents> agent_group, int& i, int argc, char** argv)
{
    if (argc - i < 2)
    {
        throw std::runtime_error("Usage: nexuslua run <name of plugin> <name of message> [<options>...]");
    }

    nexuslua::AgentMessage            message    = GetParameters(agent_group, i, argv);
    nexuslua::LuaTable::nested_tables parameters = message.GetParameterDescriptions();
    nexuslua::LuaTable                params     = pluginResult;

    for (; i < argc - 1 && cbeam::container::key_exists(parameters, std::string(argv[i])); i += 2)
    {
        const nexuslua::LuaTable& p = parameters[std::string(argv[i])];

        if (p.get_mapped_value_or_default<cbeam::container::xpod::type_index::string>("type") == "enum")
        {
            auto it = p.sub_tables.find("values");

            if (it != p.sub_tables.end() && !cbeam::container::key_exists(it->second.data, std::string(argv[i + 1])))
            {
                std::cout << std::string("Value '") + argv[i + 1] + "' not supported for parameter '" + argv[i] + "' of plugin message '" + message.GetDisplayName() + "'. Allowed values: " << std::endl
                          << cbeam::convert::to_string(parameters[std::string(argv[i])].sub_tables["values"].data);

                throw std::runtime_error("error");
            }
        }
        params.data[std::string(argv[i])] = (std::string)argv[i + 1];
    }

    params.SetReplyTo(agentName, messageName);

    CBEAM_LOG_DEBUG("Running plugin '" + message.GetAgentName() + "', message '" + message.GetMessageName() + "'");

    std::cout << "Running... " << std::endl;

    messageProcessed = false;
    std::unique_lock<std::mutex> lock(mtx);
    message.Send(params);
    cv.wait(lock, []
            { return messageProcessed; });

    if (!pluginSuccess)
    {
        throw std::runtime_error("Response from agent '" + message.GetAgentName() + "', message '" + message.GetMessageName() + "': " + pluginMessage);
    }

    if (pluginMessage.empty())
    {
        std::cout << "Finished." << std::endl;
    }
    else
    {
        std::cout << "Finished: " << pluginMessage << std::endl;
    }
}

bool contains_gtest_args(int argc, char** argv)
{
    while (--argc >= 0)
    {
        if (((std::string)argv[argc]).find("--gtest_") == 0)
        {
            return true;
        }
    }

    return false;
}

const nexuslua::LuaTable CreateArrayTable(const std::string& tableName, const std::vector<std::string> arrayValues, const int startIndex)
{
    nexuslua::LuaTable arrayTable;

    for (int i = 0; i < (int)arrayValues.size(); ++i)
    {
        arrayTable.sub_tables[tableName].data[i - startIndex] = arrayValues[i];
    }

    return arrayTable;
}

const nexuslua::LuaTable CreateArgTable(char** argv, int argc, int script)
{
    return CreateArrayTable("arg"s,
                            std::vector<std::string>(argv, argv + argc),
                            script);
}

void print_usage(std::ostream& out)
{
    out << R"END(
Usage: nexuslua <command> [options...]

Commands:
    -h, --help
        Print this help message.

    -v
        Print version and license information.

    nexuslua -e "<code>"
        Execute a string of nexuslua code.
        Example: nexuslua -e "print('hello')"

    nexuslua /path/to/your/script.lua
        Run a nexuslua script.

    nexuslua help [plugin] [message]
        Get help on available plugins and messages.
        Example (list all): nexuslua help
        Example (specific): nexuslua help "acrion image tools" CallOpenImageFile

    nexuslua run <plugin> <message> [options...]
        Run a sequence of specific plugin messages from the command line.
        In this example, an image is loaded, inverted, and saved:
            nexuslua run "acrion image tools" CallOpenImageFile path /path/to/input.jpg \
                     run "acrion image tools" CallInvertImage \
                     run "acrion image tools" CallSaveImageFile path /path/to/output.jpg
        Note that you can also use plugins directly from your nexuslua scripts.

Note: A REPL (Read-Eval-Print Loop) is not yet implemented.
)END";
}

int process_cmd_line(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << nexuslua::description::GetCopyright() << std::endl;
        print_usage(std::cout);
        return 0;
    }

    const std::string first_arg = argv[1];
    if (first_arg == "-h"s || first_arg == "--help"s)
    {
        print_usage(std::cout);
        return 0;
    }

    if (argc == 2 && first_arg == "-v"s)
    {
        std::cout << nexuslua::description::GetVersionsAndLicenses() << std::endl;
        return 0;
    }

    std::shared_ptr<nexuslua::agents> agent_group = std::make_shared<nexuslua::agents>();

    try
    {
        if (argc == 3 && argv[1] == "-e"s)
        {
            agent_group->Add("main", std::filesystem::path(), argv[2]);
        }
        else
        {
            for (int i = 1; i < argc;)
            {
                const std::string cmd(argv[i]);

                if (cbeam::convert::to_lower(std::filesystem::path(cmd).extension().string()) == ".lua")
                {
                    agent_group->Add("main"s, std::filesystem::absolute(cmd), ""s, CreateArgTable(argv, argc, i));
                    break;
                }
                else if (cmd == "help")
                {
                    ++i;
                    help(agent_group, i, argc, argv);
                }
                else if (cmd == "run")
                {
                    ++i;
                    if (!agent_group->GetAgent(agentName))
                    {
                        agent_group->Add(agentName, [](const std::shared_ptr<nexuslua::Message>& resultMessage)
                                         { HandleResult(resultMessage); });
                        agent_group->AddMessageForCppAgent(agentName, messageName);
                    }

                    run(agent_group, i, argc, argv);
                }
                else
                {
                    std::cerr << "Error: Unknown command '" << cmd << "'." << std::endl;
                    print_usage(std::cerr);
                    return 1;
                }
            }
        }

        // Above `Add` calls are synchronous; they fully execute the script and only return once the script has run to completion.
        // At this point, we must wait for any asynchronous work the script may have initiated via messages.
        // This wait is critical for two distinct scenarios:
        // 1. For scripts that sent messages: This call blocks until all resulting agent tasks are finished and the message queue is empty.
        // 2. For scripts that sent no messages: The queue is already empty.
        agent_group->WaitUntilMessageQueueIsEmpty();
        agent_group->ShutdownAgents();
        CBEAM_LOG_DEBUG("detected empty message queue");
    }
    catch (const std::exception& ex)
    {
        CBEAM_LOG(ex.what());
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

int main(int argc, char** argv)
{
#ifdef GENERATED_WITH_DBG_CONFIGURATION
    if (argc == 1 || contains_gtest_args(argc, argv)) // only do gtests if no arguments or gtest arguments have been passed
    {
        testing::InitGoogleTest(&argc, argv);
        int gtest_result = RUN_ALL_TESTS();
        if (argc > 1 || gtest_result == 1)
        {
            // If gtest arguments have been passed or tests failed, report the test result
            return gtest_result;
        }
    }
#endif

    const int result = process_cmd_line(argc, argv);
    return result;
}
