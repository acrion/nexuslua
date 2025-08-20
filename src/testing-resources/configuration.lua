#!/usr/bin/env nexuslua
local config = getconfig()
config.demo_entry1 = "my string"
config.demo_entry2 = 42
config.demo_key = {}
config.demo_key.demo_entry3 = 3.14
printtable(config)
setconfig(config)
