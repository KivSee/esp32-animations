protoc -I=. -I=../.pio/libdeps/default/KivseeRender/proto --nanopb_out=. --python_out=. animation.proto

mv animation.pb.c ../src
mv animation.pb.h ../include
