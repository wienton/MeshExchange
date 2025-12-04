gcc mongodb.c -o mongo \                                                                                                                                                              ℂ -gcc  21:33 
        -I/usr/include/libmongoc-1.0 \
        -I/usr/include/libbson-1.0 \
        -L/usr/lib/x86_64-linux-gnu \
        -lmongoc-1.0 -lbson-1.0 -pthread -lrt