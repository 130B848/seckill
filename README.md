# SecKill
* NOTICE:  ./ means the corresponding project's root directory
* Now we shall start three Redis on ports 6379, 6380 and 6381. Tmux is recommended for spliting windows
* The HTTP port was changed to 80, just send request to IP/localhost without ":port" is OK.

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
# start redis-server in other windows or use & to run in background
redis-server --port 6379 # Use *redis-cli -p 6379* to FLUSHALL if you want to clear and return to initial status
redis-server --port 6380
redis-server --port 6381
./seckill
```

## Interface Test
```python
#!/usr/bin/python
import requests
requests.get('http://localhost/seckill/getUserAll').text
requests.get('http://localhost/seckill/getCommodityAll').text
requests.get('http://localhost/seckill/seckill?user_id=e3351fa5-e422-41db-82b1-888881309cfb&commodity_id=a6bdd278-138c-4f11-bc71-47da49e74b4e').text
requests.get('http://localhost/seckill/getOrderAll').text
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
wrk -t12 -c400 -d30s "http://localhost/seckill/seckill?user_id=e3351fa5-e422-41db-82b1-888881309cfb&commodity_id=a6bdd278-138c-4f11-bc71-47da49e74b4e"
```
* Install *siege*
```bash
#!/bin/bash
sudo apt update
sudo apt install autoconf automake libtool
git clone https://github.com/JoeDog/siege.git
cd siege
./utils/bootstrap
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
