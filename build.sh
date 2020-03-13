reset
g++ -std=gnu++17 -fconcepts -fmax-errors=4   libav_2.cpp -o test -Wall   -lavformat -lavcodec -lavfilter -lavutil -lswscale     && ./test