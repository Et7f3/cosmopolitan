FROM alpine:latest

RUN apk update && apk upgrade
RUN apk add git bash make clang gcc g++ linux-headers

# https://www.kernel.org/doc/html/latest/admin-guide/binfmt-misc.html
RUN mount binfmt_misc -t binfmt_misc /proc/sys/fs/binfmt_misc
