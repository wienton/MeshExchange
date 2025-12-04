echo "start builder"
gcc mongodb.c logs/dblogs.c -o mongo -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -L/usr/lib/x86_64-linux-gnu -lmongoc-1.0 -lbson-1.0 -pthread -lrt

echo "success build binary mongo"