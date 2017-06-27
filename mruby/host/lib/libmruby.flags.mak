MRUBY_CFLAGS = -g -std=gnu99 -O3 -Wall -Werror-implicit-function-declaration -Wdeclaration-after-statement -Wwrite-strings -I"/home/mark-lee/h2o/deps/mruby/include"
MRUBY_LDFLAGS =  -L/home/mark-lee/h2o/mruby/host/lib
MRUBY_LDFLAGS_BEFORE_LIBS = 
MRUBY_LIBS = -lmruby -lm -lpthread -lreadline -lncurses
MRUBY_LIBMRUBY_PATH = /home/mark-lee/h2o/mruby/host/lib/libmruby.a
