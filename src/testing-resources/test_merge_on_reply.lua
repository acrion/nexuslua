#!/usr/bin/env nexuslua

function ReplyTest(parameters)
    local result = "Hello " .. parameters.myName
    return {MyReply=result}
end

function ReceiveReply(parameters)
    print(parameters.MyReply)
    print()
    printtable(parameters)
end

addmessage("ReplyTest")
addmessage("ReceiveReply")

send("main", "ReplyTest", {myName="nexuslua", reply_to={message="ReceiveReply", merge={MergedEntry=42, MergedSubTable={AnotherMergeEntry="test"}}}})
