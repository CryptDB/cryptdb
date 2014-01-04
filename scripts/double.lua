-- This code is ugly; but it is largely an attempt to deal with disparate
-- datastructures that are essentially isomorphic with a minimal amount
-- of looping.

-- wordpress testing hack
local new_session = true

package.cpath = package.cpath .. ";/usr/local/lib/lua/5.1/?.so"
package.path  = package.path .. ";/usr/local/share/lua/5.1/?.lua"

local CRYPTDB_DIR = os.getenv("EDBDIR")
local get_locklib =
    assert(package.loadlib(CRYPTDB_DIR .. "/obj/scripts/locklib.so",
                           "luaopen_locklib"))
get_locklib()
local luasql = assert(require("luasql.mysql"))
local proto  = assert(require("mysql.proto"))
local lanes  = assert(require("lanes"))
if require("lanes").configure then
    lanes.configure()
end
local linda  = lanes.linda()

local LOCK_FILE          = "cdb.lock"
local LOG_FILE_PATH      = CRYPTDB_DIR .. "/logs/double.log"
local cryptdb_lane       = nil
local log_file_h         = nil

-- we don't want crosstalk between different instances of 'double'
local RESULTS_QUEUE      = "results_" .. math.random(10000)
local QUERY_QUEUE        = "query_" .. math.random(10000)

function connect_server()
    print("Double Connection.")
    -- initialize pre-emptive thread
    local cryptdb_lane_gen =
        lanes.gen("*", {required = {'luasql.mysql',}}, exec_q)
    cryptdb_lane = cryptdb_lane_gen()

    -- open log file
    log_file_h = assert(io.open(LOG_FILE_PATH, "a"))
end

function disconnect_client()
    if log_file_h then
        log_file_h:close()
    end
end

function read_query(packet)
    local query  = string.sub(packet, 2)

    -- don't acquire lock or do anything with cryptdb if the query is blank
    -- > must be handled seperately because a blank query doesn't properly
    -- trigger 'read_query_result(...)'
    if 0 == string.len(query:gsub("%s+", "")) then
        return nil
    end

    -- HACK: turns off strict mode for wordpress because
    -- the testing database runs in strict mode.
    -- > Don't send to CryptDB.
    if true == new_session then
        mode = "SET @@session.sql_mode := ''"
        proxy.queries:append(1337, string.char(proxy.COM_QUERY) .. mode,
                             {resultset_is_needed = true})
        new_session = false
    end

    local cryptdb_query
    -- acquire lock and build queries
    status, lock_fd = locklib.acquire_lock(LOCK_FILE)
    if false == status then
        print("Swallowed Query: [" ..  query .. "]")
        return nil
    else
        if string.byte(packet) == proxy.COM_INIT_DB then
            cryptdb_query = "USE " .. query
        else
            cryptdb_query = query
        end

        proxy.queries:append(42, packet, {resultset_is_needed = true})
    end

    -- Clear the queues
    linda:set(QUERY_QUEUE)
    linda:set(RESULTS_QUEUE)

    -- Send the new query
    linda:send(QUERY_QUEUE, cryptdb_query)
    return proxy.PROXY_SEND_QUERY
end

function read_query_result(inj)
    local client_name = proxy.connection.client.src.name
    local query = string.sub(inj.query, 2)

    local out_status = nil

    if nil == lock_fd then
        if log_file_h then
            log_file_h:write(create_log_entry(client_name, query, false,
                                             false,
                                             "lock acquisition failed"))
            log_file_h:flush()
        end

        return
    end

    -- > somemtimes this is a table (ie SELECT), sometimes it's nil (query
    --   error), sometimes it's number of rows affected by command
    key, cryptdb_results = linda:receive(7.0, RESULTS_QUEUE)
    local cryptdb_error = not cryptdb_results
    local regular_error =  proxy.MYSQLD_PACKET_ERR ==
                                inj.resultset.query_status

    if regular_error or cryptdb_error then
        out_status = "error"
    elseif "string" == type(cryptdb_results) then
        out_status = cryptdb_results
    elseif "number" == type(cryptdb_results) then
        -- WARN: this is always going to give a nonsensical result
        -- for UPDATE and DDL queries.
        out_status =
            get_match_text(cryptdb_results == inj.resultset.affected_rows)
    elseif nil == inj.resultset.rows then
        out_status = "no result data from regular db"
    else
        -- do the naive comparison while gathering regular results,
        -- then if it fails, do the slow comparison
        local regular_results = {}

        -- HACK/CARE: double iteration
        local index = 1
        local no_fast_match = false
        for regular_row in inj.resultset.rows do
            if false == no_fast_match then
                -- don't do table_test if we already know we dont have
                -- a fast match
                if false == table_test(cryptdb_results[index], regular_row) then
                    no_fast_match = true
                end
            end

            regular_results[index] = regular_row
            index = index + 1
        end

        if #regular_results ~= #cryptdb_results then
            out_status = get_match_text(false)
        elseif false == no_fast_match then
            out_status = get_match_text(true)
        else
            -- do slow, unordered matching
            local test = slow_test(regular_results, cryptdb_results)
            out_status = get_match_text(test)
        end
    end

    if log_file_h then
        log_file_h:write(create_log_entry(client_name, query,
                                          cryptdb_error, regular_error,
                                          out_status))
        log_file_h:flush()
    end

    -- release lock
    assert(lock_fd)
    locklib.release_lock(lock_fd)
    lock_fd = nil
end

-- never returns (sends messages)
function exec_q()
    -- init
    init = function ()
        local luasql = require("luasql.mysql")
        if nil == luasql then
            return nil
        end

        local env = luasql.mysql()
        if nil == env then
            return nil
        end

        return env:connect("", "root", "letmein", "127.0.0.1", 3307)
    end

    local c = init()
    -- failure loop
    if nil == c then
        io.stderr:write("\nERROR: failed to connect to cryptdb\n\n")
        while true do
            local _ = linda:receive(1.0, QUERY_QUEUE)
            linda:send(RESULTS_QUEUE, nil)
        end
    end

    -- query/parse loop
    while true do
        -- get query
        local key, query = linda:receive(1.0, QUERY_QUEUE)

        if nil ~= query then
            -- do query
            local cursor = c:execute(query)

            -- gather results
            local result = {}
            if nil == cursor then
                result = nil
            elseif "number" == type(cursor) then
                result = cursor
            else
                local row = cursor:fetch({}, "n")
                local index = 1
                while row do
                    result[index] = row
                    row = cursor:fetch({}, "n")

                    index = index + 1
                end
            end

            linda:send(RESULTS_QUEUE, result)
        end
    end
end


function create_log_entry(client, query, cryptdb_error, regular_error,
                          status)
    return client .. "," .. csv_escape(query) .. "," .. os.date("%c") .. "," .. ppbool(cryptdb_error) .. "," .. ppbool(regular_error) .. "," .. status .. "\n"
end

function table_test(results_a, results_b)
    if type(results_a) == "nil" or type(results_b) == "nil" then
        return false
    end

    if table.getn(results_a) ~= table.getn(results_b) then
        return false
    end

    for i = 1, #results_a do
        if results_a[i] ~= results_b[i] then
            return false
        end
    end

    return true
end

-- FIXME: Can get 2x speed up by removing matched elements from the inner
-- lookup array.
function slow_test(results_a, results_b)
    if table.getn(results_a) ~= table.getn(results_b) then
        return false
    end

    for a_index = 1, #results_a do
        local matched = false
        for b_index = 1, #results_b do
            if table_test(results_a[a_index], results_b[b_index]) then
                matched = true
                break
            end
        end

        if false == matched then
            return false
        end
    end

    return true
end

function get_match_text(b)
    if true == b then
        return "matched"
    else
        return "diverged"
    end
end

function ppbool(b)
    if true == b then
        return "true"
    else
        return "false"
    end
end

-- FIXME: Implement this if it actually matters; will make code slower.
-- VAPORWARE
function csv_escape(string)
    return string
end
