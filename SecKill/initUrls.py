#!/usr/bin/python
file = open('./SecKill/user_intensive.url', 'w+')

for i in xrange(1, 1001):
    for j in xrange(1, 1001):
        file.write('http://localhost:7890/seckill/seckill?user_id=' + str(i) + '&commodiy_id=' + str(j) + '\n')

file.close()

file = open('./SecKill/commodity_intensive.url', 'w+')

for i in xrange(1, 1001):
    for j in xrange(1, 1001):
        file.write('http://localhost:7890/seckill/seckill?user_id=' + str(j) + '&commodiy_id=' + str(i) + '\n')

file.close()
