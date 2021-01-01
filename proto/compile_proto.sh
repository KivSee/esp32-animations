PROTOC_GEN_TS_PATH="/home/amir/.config/yarn/global/node_modules/.bin/protoc-gen-ts"

protoc \
    --plugin="protoc-gen-ts=${PROTOC_GEN_TS_PATH}" \
    -I=. \
    -I=../.pio/libdeps/default/KivseeRender/proto \
    --nanopb_out=. \
    --python_out=. \
    --js_out=import_style=commonjs,binary:. \
    --ts_out=. \
    animation.proto

mv animation.pb.c ../src
mv animation.pb.h ../include
