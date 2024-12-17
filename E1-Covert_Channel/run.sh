#! /bin/sh
make
./sender & 
./receiver
./err 1000