**True, hardware-level multithreading for Lua through an asynchronous agent model.**

nexuslua is a command-line interpreter that extends the Lua programming language with a powerful concurrency model built
on agents and asynchronous messaging. While standard Lua uses coroutines for cooperative multitasking, nexuslua
leverages real OS-level threads, allowing you to parallelize CPU-bound tasks and design complex, decoupled applications
with ease.

This project is the runtime executable for the [nexuslua library](https://github.com/acrion/nexuslua-library) and serves
as a reference implementation and command-line tool for running nexuslua scripts and plugins. It is the core technology
behind the desktop application [acrionphoto](https://github.com/acrion/photo).

> **Watch the Talk:** For a deep dive into the original concepts, check out the presentation from the
**[Lua Workshop 2022](https://www.youtube.com/watch?v=Y6wz6Qk475E)** (under the project's former name, "acrionlua").

---

## Tutorial: From 10 Seconds to 1 Second

The best way to understand nexuslua is to see it in action. We'll take a simple, CPU-bound task—finding prime numbers in
a large range—and progressively parallelize it using nexuslua's features.

### Step 1: The Baseline (Plain Lua)

First, let's see how a standard Lua script performs. This code checks 50,000 numbers for primality in a single-threaded
loop.

**`demo1.lua`**

```lua
#!/usr/bin/env nexuslua

local nCheckedPrimes = 0
local count = 0
local startTime = time()

function IsPrime(number)
    local q = math.sqrt(number)
    local found = true
    for k = 3, q, 2 do
        if number % k == 0 then
            found = false
            break
        end
    end
    return found
end

local n1 = 10000000001
local n2 = 10000100001

print("Checking prime numbers between ", n1, " and ", n2)

for i = n1, n2, 2 do
    if IsPrime(i) then
        count = count + 1
    end
    nCheckedPrimes = nCheckedPrimes + 1
end

local endTime = time()
print("Checked ", nCheckedPrimes, " numbers in ", (endTime - startTime) / 1.0e8, " seconds, found ", count, " prime")
```

**Execution:**

```sh
$ nexuslua demo1.lua
Checked 50001 numbers in 10.03 seconds, found 4306 prime
```

As expected, it's slow. The entire calculation runs on a single core.

### Step 2: Introducing Asynchronous Messages

Now, let's introduce nexuslua's messaging. We'll convert `IsPrime` into a message handler. The main script will `send` a
message for each number to be checked.

**`demo2.lua`**

```lua
#!/usr/bin/env nexuslua

local nRequests = 0
local nCheckedPrimes = 0
local count = 0
local startTime = time()

function IsPrime(parameters)
    -- ... (same implementation as before) ...
    return { isPrime = found }
end

function CountPrime(parameters)
    nCheckedPrimes = nCheckedPrimes + 1
    if parameters.isPrime then
        count = count + 1
    end
    if nCheckedPrimes == nRequests then
        local endTime = time()
        print("Checked ", nCheckedPrimes, " numbers in ", (endTime - startTime) / 1.0e8, " seconds, found ", count, " prime")
    end
end

addmessage("IsPrime")
addmessage("CountPrime")

local n1, n2 = 10000000001, 10000100001
print("Checking prime numbers between ", n1, " and ", n2)

for i = n1, n2, 2 do
    -- Send a message to ourself to check a number.
    -- The result will be sent to the "CountPrime" message handler.
    send("main", "IsPrime", { number = i, reply_to = { message = "CountPrime" } })
    nRequests = nRequests + 1
end
```

**Execution:**

```sh
$ nexuslua demo2.lua
Checked 50001 numbers in 8.44 seconds, found 4306 prime
```

It's a bit faster, but not by much. **Why?** The main script still runs in a single thread. It loops through all 50,000
numbers and queues up the messages. Only *after* the script finishes does nexuslua process the queued messages in
parallel. This is an anti-pattern but illustrates a key concept: to achieve true concurrency, the work must be initiated
from a separate, parallel context.

### Step 3: Structuring for Parallelism

Let's refactor the code to prepare for true parallelism. We'll move the loop into its own function, `RequestPrimes`,
which can be triggered by a single message.

**`demo3.lua`**

```lua
#!/usr/bin/env nexuslua

-- ... (IsPrime and CountPrime functions remain the same) ...

function RequestPrimes(parameters)
    print("Checking prime numbers between ", parameters.n1, " and ", parameters.n2)
    for i = parameters.n1, parameters.n2, 2 do
        send("main", "IsPrime", { number = i, reply_to = { message = "CountPrime" } })
        nRequests = nRequests + 1
    end
end

addmessage("IsPrime")
addmessage("CountPrime")
addmessage("RequestPrimes")

-- Trigger the whole process with a single message
send("main", "RequestPrimes", { n1 = 10000000001, n2 = 10000100001 })
```

**Execution:**

```sh
$ nexuslua demo3.lua
Checked 50001 numbers in 8.04 seconds, found 4306 prime
```

The performance is similar, but the design is now ready. We have decoupled the *request* for work from the *execution*
of the work.

### Step 4: Full Power with Agents

This is where nexuslua shines. We'll create a dedicated `numbers` **agent** to handle the `IsPrime` checks. This agent
runs in its own OS thread, completely in parallel with the `main` script.

**`demo5.lua`**

```lua
#!/usr/bin/env nexuslua

local nRequests, nCheckedPrimes, count = 0, 0, 0
local startTime = time()

function CountPrime(parameters)
    nCheckedPrimes = nCheckedPrimes + 1
    if parameters.isPrime then count = count + 1 end
    if nCheckedPrimes == nRequests then
        local endTime = time()
        print("Checked ", nCheckedPrimes, " numbers in ", (endTime - startTime) / 1.0e8, " seconds, found ", count, " prime")
    end
end

function RequestPrimes(parameters)
    local maxThreads = (cores() + 1) // 2
    print("Checking prime numbers between ", parameters.n1, " and ", parameters.n2, " using " .. maxThreads .. " threads.")
    for i = parameters.n1, parameters.n2, 2 do
        -- Send the work to the dedicated "numbers" agent
        send("numbers", "IsPrime", { number = i, threads = maxThreads, reply_to = { agent = "main", message = "CountPrime" } })
        nRequests = nRequests + 1
    end
end

if not isreplicated() then
    -- Define the agent's code as a string
    local numbersAgentCode = [==[
        function IsPrime(parameters)
            -- ... (same prime checking logic) ...
            return { isPrime = found }
        end
    ]==]

    -- Create the "numbers" agent. It immediately runs in a new thread.
    addagent("numbers", numbersAgentCode, { "IsPrime" })
    
    addmessage("CountPrime")
    addmessage("RequestPrimes")

    -- Kick off the process
    send("main", "RequestPrimes", { n1 = 10000000001, n2 = 10000100001 })
end
```

**Execution:**

```sh
$ nexuslua demo5.lua
Checked 50001 numbers in 0.84 seconds, found 4306 prime
```

*Voilà!* A **12x speedup**. Here’s what happened:

1. The `main` script creates the `numbers` agent, which starts running in a new thread.
2. `main` sends a single message to itself to start `RequestPrimes`.
3. The `RequestPrimes` loop now sends messages to the `numbers` agent, which is already running and ready to process
   them in parallel.
4. The `threads` parameter in `send()` tells the `numbers` agent it can replicate itself (create more threads) up to
   `maxThreads` to handle the workload, massively parallelizing the prime checks.
5. Each time a `numbers` agent finishes, it returns a result, which nexuslua automatically sends back to `main`'s
   `CountPrime` function for aggregation.

This tutorial shows the core power of nexuslua: designing applications as a set of independent, message-driven agents
that can run concurrently.

---

## The Power of Decoupled Design

Even if you don't need maximum parallelism, the agent model encourages a cleaner software architecture. Components
communicate via well-defined messages instead of direct function calls. This decoupling makes your code more modular,
easier to maintain, and naturally responsive, as no single component blocks another.

---

## Extending nexuslua with Plugins

Installed plugins are just **agents** addressable by name. While the CLI `run` chain (see below section *Usage*) is handy for quick tests, the idiomatic way to use plugins is to send asynchronous messages from a nexuslua script. This keeps your workflow non-blocking and composable.

### Message shapes and why you must pass parameters forward

Plugin replies have a consistent envelope:

- The **payload returned by the plugin** (e.g. `imageBuffer`, `width`, `height`, `channels`, `depth`, …) may appear **at the top level** of the reply table **or** the plugin may perform work **in place** and return no payload.
- In all cases the reply also carries:
```

original_message {
message_name = "<MessageNameYouSent>"
parameters   = { ...the parameters you sent... }
}

```
- Some messages (e.g. **`CallOpenImageFile`**) return image fields **at the top level**.
- Others (e.g. **`CallInvertImage`**) work **in place** and the next step must re-use the image fields from
**`original_message.parameters`**.

> **Debugging tip:** call `printtable(p)` inside your handlers to inspect the exact shape you get back.

### Example: open → invert → save (asynchronous, minimal)

The script below opens an image through the [acrion image tools](https://github.com/acrion/image-tools) plugin, inverts it in place, and then saves it.  
It intentionally demonstrates both cases: payload at **top level** (Open) vs **in-place** (Invert).

```lua
#!/usr/bin/env nexuslua
-- Usage: nexuslua invert-and-save.lua <input> <output>

local function merge(a, b)
local t = {}
for k,v in pairs(a or {}) do t[k] = v end
for k,v in pairs(b or {}) do t[k] = v end
return t
end

function OnLoaded(img)
-- printtable(img)  -- inspect the reply if needed
print("Image loaded; inverting...")
send("acrion image tools", "CallInvertImage",
     merge(img, { reply_to = { agent = "main", message = "OnInverted" } }))
end

function OnInverted(p)
-- printtable(p)  -- Invert works in place: use original_message.parameters
local out = arg[2]
print("Image inverted; saving to " .. out .. "...")
send("acrion image tools", "CallSaveImageFile",
     merge(p.original_message.parameters, { path = out,
                                            reply_to = { agent = "main", message = "OnSaved" } }))
end

function OnSaved(p)
-- printtable(p)
if p and p.error then
  print("Error saving: " .. tostring(p.error))
else
  print("Workflow complete! Wrote " .. p.original_message.parameters.path)
end
end

addmessage("OnLoaded")
addmessage("OnInverted")
addmessage("OnSaved")

send("acrion image tools", "CallOpenImageFile", {
path = arg[1],
reply_to = { agent = "main", message = "OnLoaded" }
})
```

**Why the `merge` calls?**
Each message must receive its required image fields **flat** in the parameter table. nexuslua won’t “chain” fields from previous replies automatically. We therefore **re-use** the returned fields (top level for Open; `original_message.parameters` for in-place messages like Invert) and **overlay** any overrides (`reply_to`, `path`) before sending the next message.

> If you see “Missing parameter value for channels”: you likely forwarded the wrong table shape. Use `printtable(p)` and pass the image fields that the plugin actually returned (either top level or `original_message.parameters`).

## Beyond Concurrency: Built-in Utility Functions

nexuslua doesn't just add multithreading; it also enriches the Lua environment with a host of useful, cross-platform
utility functions. These functions simplify common tasks and are available globally in any nexuslua script.

Here are a few examples:

- [import()](https://nexuslua.org/import.html): Utilize functions from shared libraries (*.dll, *.so, *.dylib) directly
  within nexuslua scripts.
- [zip(source_dir, archive_path)](https://nexuslua.org/zip.html) / [unzip(archive_path, target_dir)](https://nexuslua.org/unzip.html):
  Built-in support for creating and extracting ZIP archives.
- [`userdatadir()`] / [`homedir()`]: Provide cross-platform paths to standard user directories.
- [env(...)](https://nexuslua.org/env.html): Read environment variables.
- [`log(message)`](https://nexuslua.org/log.html): Writes a message to the `nexuslua.log` file in the [user data directory](https://cbeam.org/doxygen/namespacecbeam_1_1filesystem.html#ae598d93475d7f8675bb85d7542cf90ab) for easy debugging.
- [`time()`](https://nexuslua.org/time.md): Returns a high-resolution timestamp, perfect for benchmarking.
- [`cores()`](https://nexuslua.org/cores.md): Detects the number of available hardware threads on the system, which is useful for configuring parallel
  tasks dynamically.

For a complete list and detailed documentation of all available functions, please visit **[https://nexuslua.org](https://nexuslua.org)**.

---

## Community

Have questions, want to share what you've built, or just say hi? Join our community on Discord!

💬 **[Join the nexuslua Discord Server](https://discord.gg/aaZPevdEen)**

---

## Plugin Manager & Marketplace Roadmap

*   The [nexuslua library](https://github.com/acrion/nexuslua-library) already supports **Install / Uninstall / Update / License** management.
*   The next step is to connect to the public registry at **[https://github.com/acrion/nexuslua-plugins](https://github.com/acrion/nexuslua-plugins)** to fetch **plugin metadata and versions** (from each plugin's `nexuslua_plugin.toml`).
*   All plugins are usable from nexuslua, of course - but only a subset is relevant to [acrionphoto](https://github.com/acrion/photo); the application filters by a plugin's `main.lua` **message descriptors** (looking for I/O, coordinate handlers, and image parameters).

## Plugin metadata (`nexuslua_plugin.toml`)

Each plugin ships a `nexuslua_plugin.toml` in its **plugin root**. For *acrion image tools* the build generates it into the plugin directory. Example:

```toml
displayName = "acrion image tools"
version = "1.0.246"
isFreeware = true
description = "A set of essential tools for basic image manipulation."
urlHelp = "https://github.com/acrion/image-tools"
urlLicense = "https://github.com/acrion/image-tools/blob/main/LICENSE"
urlDownloadLinux = "https://github.com/acrion/image-tools/releases/download/1.0.246/image-tools-Linux.zip"
urlDownloadWindows = "https://github.com/acrion/image-tools/releases/download/1.0.246/image-tools-Windows.zip"
#urlDownloadDarwin = "https://github.com/acrion/image-tools/releases/download/1.0.246/image-tools-Darwin.zip"
```

Notes:

* `urlDownloadDarwin` is commented out because the macOS build of the [acrion image tools plugin](https://github.com/acrion/image-tools) is temporarily unavailable during refactoring.
* If `isFreeware = false`, the [acrionphoto](https://github.com/acrion/photo) Plugin Manager shows two extra buttons:

  * **"Get License Key…"** → opens the system browser at `urlPurchase`.
  * **"Install key or other files…"** → lets you select files to copy into the plugin’s `persistent/` folder (survives updates/uninstall).
* In the forthcoming public registry (`acrion/nexuslua-plugins`), the central list stores **only URLs** to each plugin’s TOML. Versioning and updates remain fully under the plugin author’s control.

---

## Build

### Standalone (recommended if you only need the CLI)

You **do not** need `nexuslua-build` to build the `nexuslua` executable.

```bash
# from repo root
cmake -B build src/
cmake --build build
```

The resulting `nexuslua` binary is in `build/` (platform-specific subfolder).

### Via the all-in-one orchestrator (optional)

If you want the full stack (including plugins & acrionphoto GUI app), you can still use the umbrella repo:

* [https://github.com/acrion/nexuslua-build](https://github.com/acrion/nexuslua-build)
  Use `--profile nexuslua` to build the core engine only, or `--profile acrionphoto` for the entire application stack.

## Usage

The `nexuslua` executable can be used to run scripts, execute code directly, or interact with plugins from the command
line.

#### General Commands

* **Print help and command line options**
  ```bash
  nexuslua -h
  # or
  nexuslua --help
  ```

* **Print version and license information**
  ```bash
  nexuslua -v
  ```

#### Executing Code

* **Run a nexuslua script file**
  ```bash
  nexuslua /path/to/your/script.lua
  ```

* **Run a string of code from the command line**
  ```bash
  nexuslua -e "print('hello')"
  ```

#### Interacting with Plugins

* **Get help on available plugins and messages**
  ```bash
  # List all available plugins and their messages
  nexuslua help
  
  # Get detailed help for a specific plugin message
  nexuslua help "acrion image tools" CallOpenImageFile
  ```

* **Run a sequence of plugin messages**

  You can chain multiple commands to create complex workflows directly from your terminal. Note that you must repeat the
  `run` keyword for each message in the sequence. This is intended for scenarios where you only want to use plugin
  functionality without additional logic.
  You have more possibilities when using plugins directly from your nexuslua scripts, particularly with regard to
  concurrency.

  ```bash
  # This example loads an image, inverts its colors, and saves it to a new file.
  nexuslua run "acrion image tools" CallOpenImageFile path /path/to/input.jpg \
           run "acrion image tools" CallInvertImage \
           run "acrion image tools" CallSaveImageFile path /path/to/output.jpg
  ```

**Note:** A REPL (Read-Eval-Print Loop) for interactive sessions is not yet implemented.
