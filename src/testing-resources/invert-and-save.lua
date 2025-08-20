#!/usr/bin/env nexuslua
-- Usage: nexuslua invert-and-save.lua <input> <output>

local function merge(a, b)
  local t = {}
  for k,v in pairs(a or {}) do t[k] = v end
  for k,v in pairs(b or {}) do t[k] = v end
  return t
end

function OnLoaded(img)
  -- printtable(img)
  print("Image loaded; inverting...")
  send("acrion image tools", "CallInvertImage",
       merge(img, { reply_to = { agent = "main", message = "OnInverted" } }))
end

function OnInverted(p)
  -- printtable(p)
  local out = arg[2]
  print("Image inverted; saving to " .. out .. "...")
  send("acrion image tools", "CallSaveImageFile",
       merge(p.original_message.parameters, { path = out, reply_to = { agent = "main", message = "OnSaved" } }))
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
