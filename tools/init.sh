#!/bin/bash

./coind -datadir=./ importprivkey Y5F2GraTdQqMbYrV6MG78Kbg4QE8p4B2DyxMdLMH7HmDNtiNmcbM
./coind -datadir=./ importprivkey Y7HWKeTHFnCxyTMtCEE6tVkqBzXoN1Yjxcx5Rs8j2dsSSvPxvF7p
./coind -datadir=./ importprivkey Y871eB5Xiss2ugKWQRb4nmMhKTnmXAEyUqBimTCupogzoSTVCSU9
./coind -datadir=./ importprivkey Y9cAUsEhfsihbePnCYYCETpN1PVovqTMX4kauKRsZ9ERdz1uumeK
./coind -datadir=./ importprivkey Y4unEjiFk1YJQi1jaT3deY4t9Hm1eSk9usCam35LcN85cUA2QmZ5
./coind -datadir=./ importprivkey Y5XKsR95ymf2pEyuhDPLtuvioHRo6ogDDNnaf4YU91ABvLb68QBU
./coind -datadir=./ importprivkey Y7diE8BXuwTkjSzgdZMnKNhzYGrU8oSk31anJ1mwipSCcnPakzTA
./coind -datadir=./ importprivkey YCjoCrtGEvMPZDLzBoY9GP3r7pqWa5mgzUxqAsVub6xnUVBwQHxE
./coind -datadir=./ importprivkey Y6bKBN4ZKBNHJZpQpqE7y7TC1QpdT32YtAjw4Me9Bvgo47b5ivPY
./coind -datadir=./ importprivkey Y8G5MwTFVsqj1FvkqFDEENzUBn4yu4Ds83HkeSYP9SkjLba7xQFX
./coind -datadir=./ importprivkey YAq1NTUKiYPhV9wq3xBNCxYZfjGPMtZpEPA4sEoXPU1pppdjSAka

./coind -datadir=./ importprivkey Y6J4aK6Wcs4A3Ex4HXdfjJ6ZsHpNZfjaS4B9w7xqEnmFEYMqQd13

./coind -datadir=. send 0-1 0-2 10000000000
./coind -datadir=. send 0-1 0-3 10000000000
./coind -datadir=. send 0-1 0-4 10000000000
./coind -datadir=. send 0-1 0-5 10000000000
./coind -datadir=. send 0-1 0-6 10000000000
./coind -datadir=. send 0-1 0-7 10000000000
./coind -datadir=. send 0-1 0-8 10000000000
./coind -datadir=. send 0-1 0-9 10000000000
./coind -datadir=. send 0-1 0-10 10000000000
./coind -datadir=. send 0-1 0-11 10000000000
./coind -datadir=. send 0-1 0-12 10000000000

sleep 20

./coind -datadir=. votedelegatetx 0-2 "[{\"delegate\":\"0-2\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-3 "[{\"delegate\":\"0-3\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-4 "[{\"delegate\":\"0-4\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-5 "[{\"delegate\":\"0-5\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-6 "[{\"delegate\":\"0-6\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-7 "[{\"delegate\":\"0-7\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-8 "[{\"delegate\":\"0-8\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-9 "[{\"delegate\":\"0-9\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-10 "[{\"delegate\":\"0-10\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-11 "[{\"delegate\":\"0-11\", \"votes\":100000000}]" 10000
./coind -datadir=. votedelegatetx 0-12 "[{\"delegate\":\"0-12\", \"votes\":100000000}]" 10000
