FROM alpine:latest
RUN apk --no-cache add musl-dev git gcc make

COPY . /tmp/mping
RUN git clone --depth=1 file:///tmp/mping /root/mping
WORKDIR /root/mping

RUN make -j5 && make install-strip

FROM alpine:latest
COPY --from=0 /usr/local/bin/mping /usr/bin/mping

CMD [ "/usr/bin/mping" ]
