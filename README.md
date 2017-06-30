# SecKill
* NOTICE:  ./ means the corresponding project's root directory

## Dependency
```bash
#!/bin/bash
sudo apt update
sudo apt install libuv1-dev libssl-dev redis-server
```

## Build and Run
```bash
#!/bin/bash
cd seckill
cmake -DWITH_BUNDLED_SSL=on .
make seckill -j8
# start redis-server in another window
./redis-server
./seckill
```

## Interface Test
```python
#!/usr/bin/python
import requests
request.get('http://localhost:7890/seckill/getUserAll').text
request.get('http://localhost:7890/seckill/getCommodityAll').text
request.get('http://localhost:7890/seckill/seckill?user_id=1&commodiy_id=1').text
request.get('http://localhost:7890/seckill/getOrderAll').text
```

## Benchmark 
* Install *wrk* with APT on Ubuntu
```bash
#!/bin/bash
sudo apt install wrk
```
* *wrk* for single kind of request
```bash
#!/bin/bash
wrk -t12 -c400 -d30s "http://localhost:7890/seckill/seckill?user_id=1&commodity_id=1"
```
* Install *siege*
```bash
#!/bin/bash
sudo apt update
sudo apt install autoconf automake libtool
git clone https://github.com/JoeDog/siege.git
cd siege
# "which openssl" to get path to ssl, here shows the default path
./configure --with-ssl=/usr/bin/openssl
make -j8
sudo make install
```
* For the first time, you need to generate test urls
```bash
#!/bin/bash
chmod +x ./SecKill/initUrls.py
./SecKill/initUrls.py
```
* *siege* for pressure test
```bash
#!/bin/bash
# sequential test
siege -c 200 -r 50000 -f ./SecKill/user_intensive.url
siege -c 200 -r 50000 -f ./SecKill/commodity_intensive.url
# random test
siege -i -c 200 -r 50000 -f ./SecKill/user_intensive.url
siege -i -c 200 -r 50000 -f ./SecKill/commodity_intensive.url
```
