
#!/bin/bash

function usage()
{

    echo "功能：部署 11 个节点，并导入私钥"
    echo ""
    echo "使用方式："
    echo "  假设预期部署目录为 TargetDir"
    echo "  1. 将可执行文件 coind，脚本 deploy.sh 拷贝到 TargetDir"
    echo "  2. 通过 ./deploy.sh -h 查看帮助信息"
    echo "      部署        ./deploy.sh deploy"
    echo "      启动服务    ./deploy.sh start"
    echo "      停止服务    ./deploy.sh stop"
    echo "      更新程序    替换 coind 后执行 ./deploy.sh update"
    echo "      清理        ./deploy.sh cleanup"
    echo "      查看帮助    ./deploy.sh -h"
    echo ""
    echo "注意事项：防火墙关闭或开放对应端口"
    echo ""
}

function stop()
{
    #echo "kill all nodes if exists"
    #ps afx|grep node|grep -v grep|awk '{print $1}'|xargs kill -9
    for i in $(seq 1 11)
    do
        Node="node"$i
        if [ -d $Node ]; then
            cd $Node
            ./$Node -datadir=./ stop
            cd ..
        fi
    done
}

function start()
{
    for i in $(seq 1 11)
    do
        Node="node"$i
        cd $Node
        ./$Node -datadir=./ &
        cd ..
    done
}

function deploy()
{
    stop

    echo "step 1: deploy all nodes"
    for i in $(seq 1 11)
    do
        Node="node"$i
        echo "begin to deploy $Node"
        rm -rf $Node
        mkdir -p $Node

        Executive="node"$i
        cp coind $Executive/$Executive

        if [ $i -lt 10 ]; then
            Uiport="450"$i
            Rpcport="690"$i
            Port="790"$i
            Connect="connect=127.0.0.1:790"$i
        else
            Uiport="45"$i
            Rpcport="69"$i
            Port="79"$i
            Connect="connect=127.0.0.1:79"$i
        fi

cat >> WaykiChain.conf << EOF
rpcuser=chain
rpcpassword=123
regtest=1
blockminsize=1000
zapwallettxes=0
debug=INFO
debug=MINER
debug=ERROR
debug=net
debug=vm
#debug=shuffle
#debug=profits
logprinttoconsole=0
logtimestamps=1
logprinttofile=1
logprintfileline=1
checkblocks=1
server=1
listen=1
uiport=$Uiport
rpcport=$Rpcport
port=$Port
rpcallowip=*.*.*.*
connect=127.0.0.1:7901
connect=127.0.0.1:7902
connect=127.0.0.1:7903
connect=127.0.0.1:7904
connect=127.0.0.1:7905
connect=127.0.0.1:7906
connect=127.0.0.1:7907
connect=127.0.0.1:7908
connect=127.0.0.1:7909
connect=127.0.0.1:7910
connect=127.0.0.1:7911
isdbtraversal=1
disablesafemode=1
genblock=1
genblocklimit=1000000
logsize=100
EOF

        sed -i "/$Connect/d" WaykiChain.conf
        mv WaykiChain.conf $Node

    done

    echo "step 2: start all nodes"
    start

    sleep 5

    echo "step 3: import private key for every node"
    cd node1 && ./node1 -datadir=./ importprivkey Y5F2GraTdQqMbYrV6MG78Kbg4QE8p4B2DyxMdLMH7HmDNtiNmcbM
    cd ..
    cd node2 && ./node2 -datadir=./ importprivkey Y7HWKeTHFnCxyTMtCEE6tVkqBzXoN1Yjxcx5Rs8j2dsSSvPxvF7p
    cd ..
    cd node3 && ./node3 -datadir=./ importprivkey Y871eB5Xiss2ugKWQRb4nmMhKTnmXAEyUqBimTCupogzoSTVCSU9
    cd ..
    cd node4 && ./node4 -datadir=./ importprivkey Y9cAUsEhfsihbePnCYYCETpN1PVovqTMX4kauKRsZ9ERdz1uumeK
    cd ..
    cd node5 && ./node5 -datadir=./ importprivkey Y4unEjiFk1YJQi1jaT3deY4t9Hm1eSk9usCam35LcN85cUA2QmZ5
    cd ..
    cd node6 && ./node6 -datadir=./ importprivkey Y5XKsR95ymf2pEyuhDPLtuvioHRo6ogDDNnaf4YU91ABvLb68QBU
    cd ..
    cd node7 && ./node7 -datadir=./ importprivkey Y7diE8BXuwTkjSzgdZMnKNhzYGrU8oSk31anJ1mwipSCcnPakzTA
    cd ..
    cd node8 && ./node8 -datadir=./ importprivkey YCjoCrtGEvMPZDLzBoY9GP3r7pqWa5mgzUxqAsVub6xnUVBwQHxE
    cd ..
    cd node9 && ./node9 -datadir=./ importprivkey Y6bKBN4ZKBNHJZpQpqE7y7TC1QpdT32YtAjw4Me9Bvgo47b5ivPY
    cd ..
    cd node10 && ./node10 -datadir=./ importprivkey Y8G5MwTFVsqj1FvkqFDEENzUBn4yu4Ds83HkeSYP9SkjLba7xQFX
    cd ..
    cd node11 && ./node11 -datadir=./ importprivkey YAq1NTUKiYPhV9wq3xBNCxYZfjGPMtZpEPA4sEoXPU1pppdjSAka
    cd ..

    echo "step 4: set node1 to mine"
    cd node1
    ./node1 -datadir=. importprivkey Y6J4aK6Wcs4A3Ex4HXdfjJ6ZsHpNZfjaS4B9w7xqEnmFEYMqQd13
    ./node1 -datadir=. getbalance
    cd ..

    echo "================================="
    echo "congratulations, all is done"
    echo "================================="
}

function update()
{
    stop

    for i in $(seq 1 11)
    do
        Node="node"$i
        echo "begin to update $Node"

        Executive="node"$i
        rm -rf $Executive/$Executive
        cp coind $Executive/$Executive
    done

    start

    echo "================================="
    echo "congratulations, all is done"
    echo "================================="
}

function cleanup()
{
    stop

    for i in $(seq 1 11)
    do
        Node="node"$i
        rm -rf $Node
    done
}

if [ $# -ne 1 ]; then
    usage
    exit 0
fi

if [ $1"x" = "-h"x -o $1"x" = "--helpx"  -o $1"x" = "helpx" ]; then
    usage
    exit 0
fi

if [ $1"x" = deploy"x" ]; then
    deploy
    exit 0
elif [ $1"x" = start"x" ]; then
    start
    exit 0
elif [ $1"x" = stop"x" ]; then
    stop
    exit 0
elif [ $1"x" = update"x" ]; then
    update
    exit 0
elif [ $1"x" = cleanup"x" ]; then
    cleanup
    exit 0
else
    echo "error: invalid argument"
    usage
    exit 0
fi

