#pragma once


namespace wasm { namespace rpc{

    const char *submit_wasm_contract_deploy_tx_rpc_help_message = R"=====(
        submitwasmcontractdeploytx "sender" "contract" "wasm_file" "abi_file" [symbol:fee:unit]
        deploy code and abi to an account as contract
        Arguments:
        1."sender":          (string required) contract owner address from this wallet
        2."contract":        (string required), contract name
        3."wasm_file":       (string required), the file path of the contract wasm code
        4."abi_file":        (string required), the file path of the contract abi
        5."symbol:fee:unit": (string:numeric:string, optional) fee paid to miner, default is WICC:100000:sawi
        Result:
        "txhash":            (string)
        Examples:
        > ./coind submitwasmcontractdeploytx "walker222222" "walker222222" "/tmp/myapp.wasm" "/tmp/myapp.abi"
        As json rpc call 
        > curl --user myusername -d '{"jsonrpc": "1.0", "id":"curltest", "method":"submitwasmcontractdeploytx", "params":["walker222222", "walker222222", "/tmp/myapp.wasm", "/tmp/myapp.bai"]}' -H 'Content-Type: application/json;' http://127.0.0.1:8332
    )=====";

    const char *submit_wasm_contract_call_tx_rpc_help_message = R"=====(
        submitwasmcontractcalltx "sender" "contract" "action" "data" "fee"
        1."sender ":         (string, required) sender name
        2."contract":        (string, required) contract name
        3."action":          (string, required) action name
        4."data":            (json string, required) action data
        5."symbol:fee:unit": (numeric, optional) pay to miner
        Result:
        "txid":       (string)
        Examples: 
        > ./coind submitwasmcontractcalltx "xiaoyu111111" "walker222222" "transfer" '["xiaoyu111111", "walker222222", "100000000 WICC","transfer to walker222222"]'
        As json rpc call 
        > curl --user myusername -d '{"jsonrpc": "1.0", "id":"curltest", "method":"setcodewasmcontracttx", "params":["xiaoyu111111", "walker222222", "transfer", '["xiaoyu111111", "walker222222", "100000000 WICC","transfer to walker222222"]']}' -H 'Content-Type: application/json;' http://127.0.0.1:8332
    )=====";

    const char *get_table_wasm_rpc_help_message = R"=====(
        gettablewasm "contract" "table" "numbers" "begin_key"
        1."contract": (string, required) contract name"
        2."table":    (string, required) table name"
        3."numbers":  (numberic, optional) numbers"
        4."begin_key":(string, optional) smallest key in Hex"
        Result:"
        "rows":       (string)"
        "more":       (bool)"
        nExamples: 
        > ./coind gettablewasm "walker222222" "accounts" 
        As json rpc call 
        > curl --user myusername -d '{"jsonrpc": "1.0", "id":"curltest", "method":"gettablewasm", "params":["walker222222", "accounts"]}' -H 'Content-Type: application/json;' http://127.0.0.1:8332
    )=====";

    const char *json_to_bin_wasm_rpc_help_message = R"=====(
        jsontobinwasm "contract" "action" "data"
        1."contract": (string, required) contract name
        2."action"  : (string, required) action name
        3."data"    : (json string, required) action data in json
        Result:
        "data":       (string in hex)
        Examples: 
        > ./coind jsontobinwasm "walker222222" "transfer" '["xiaoyu111111", "walker222222", "100000000 WICC","transfer to walker222222"]'
        As json rpc call 
        > curl --user myusername -d '{"jsonrpc": "1.0", "id":"curltest", "method":"jsontobinwasm", "params":["walker222222","transfer",'["xiaoyu111111","walker222222", "100000000 WICC", "transfer to walker222222"]']}' -H 'Content-Type: application/json;' http://127.0.0.1:8332
    )=====";

    const char *bin_to_json_wasm_rpc_help_message = R"=====(
        bintojsonwasm "contract" "action" "data"
        1."contract": (string, required) contract name
        2."action"  : (string, required) action name
        3."data"    : (binary hex string, required) action data in hex
        Result:
        "data":       (string in json)
        Examples: 
        > ./coind bintojsonwasm "walker222222" "transfer" "000000809a438deb000000000000af91809698000000000004454f5300000000107472616e7366657220746f206d61726b"
        As json rpc call 
        > curl --user myusername -d '{"jsonrpc": "1.0", "id":"curltest", "method":"bintojsonwasm", "params":["walker222222","transfer", "000000809a438deb000000000000af91809698000000000004454f5300000000107472616e7366657220746f206d61726b"]}' -H 'Content-Type: application/json;' http://127.0.0.1:8332
    )=====";

    const char *get_code_wasm_rpc_help_message = R"=====(
        getcodewasm "contract" 
        1."contract": (string, required) contract name
        Result:
        "code":        (string in hex)
        Examples:
        > ./coind getcodewasm "walker222222" 
        As json rpc call 
        > curl --user myusername -d '{"jsonrpc": "1.0", "id":"curltest", "method":"getcodewasm", "params":["walker222222"]}' -H 'Content-Type: application/json;' http://127.0.0.1:8332
    )=====";

    const char *get_abi_wasm_rpc_help_message = R"=====(
        getabiwasm "contract" 
        1."contract": (string, required) contract name
        Result:
        "code":        (string)
        Examples:
        > ./coind getabiwasm "walker222222" 
        As json rpc call 
        > curl --user myusername -d '{"jsonrpc": "1.0", "id":"curltest", "method":"getabiwasm", "params":["walker222222"]}' -H 'Content-Type: application/json;' http://127.0.0.1:8332
    )=====";

    const char *get_tx_trace_rpc_help_message = R"=====(
        gettxtrace "txid" 
        1."txid": (string, required)  The hash of transaction
        Result an object of the transaction detail
        "nResult":
        Examples:
        > ./coind gettxtrace "wTtCsc5X9S5XAy1oDuFiEAfEwf8bZHur1W" 
        As json rpc call 
        > curl --user myusername -d '{"jsonrpc": "1.0", "id":"curltest", "method":"gettxtrace", "params":"wTtCsc5X9S5XAy1oDuFiEAfEwf8bZHur1W"}' -H 'Content-Type: application/json;' http://127.0.0.1:8332
    )=====";

} // rpc
} // wasm
