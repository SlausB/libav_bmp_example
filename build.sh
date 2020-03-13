reset
g++ -std=gnu++17 -fconcepts main.cpp -o test -Wall   -lavformat -lavcodec -lavfilter -lavutil -lswscale -lswresample     && ./test