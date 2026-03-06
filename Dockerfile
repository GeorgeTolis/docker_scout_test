FROM alpine

RUN apk add gcc make git linux-headers musl-dev

RUN git clone https://github.com/HewlettPackard/wireless-tools/ && cd wireless-tools/wireless_tools && make CFLAGS='-Wno-error -Wno-implicit-function-declaration -Wno-int-conversion'
