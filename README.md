# SecKill

## Dependency (wrk needs Ubuntu's APT)
```bash
#!/bin/bash
sudo apt update
sudo apt install libuv1-dev libssl-dev redis-server wrk
```

## Build and Run
```bash
#!/bin/bash
cd seckill
cmake -DWITH_BUNDLED_SSL=on .
make seckill -j8
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
```bash
#!/bin/bash
wrk -t12 -c400 -d30s "http://localhost:7890/seckill/seckill?user_id=1&commodity_id=1"
```
