#!/usr/bin/python
input = open('./SecKill/input.txt', 'r')

users = []
commodities = []

user_num = int(input.readline())
for i in xrange(user_num):
    users.append(input.readline().split(',')[0])

commodity_num = int(input.readline())
for i in xrange(commodity_num):
    commodities.append(input.readline().split(',')[0])

input.close()

file = open('./SecKill/user_intensive.url', 'w+')

for i in users:
    for j in commodities:
        file.write('http://localhost:7890/seckill/getCommodityById?commodity__id=' + j + '\n')
        file.write('http://localhost:7890/seckill/seckill?user_id=' + i + '&commodiy_id=' + j + '\n')

file.close()

file = open('./SecKill/commodity_intensive.url', 'w+')

for i in users:
    for j in commodities:
        file.write('http://localhost:7890/seckill/getCommodityById?commodity__id=' + i + '\n')
        file.write('http://localhost:7890/seckill/seckill?user_id=' + j + '&commodiy_id=' + i + '\n')

file.close()
