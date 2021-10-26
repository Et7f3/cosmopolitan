FROM alpine:latest

RUN apk update && apk upgrade
RUN apk add git bash make clang gcc g++ linux-headers

