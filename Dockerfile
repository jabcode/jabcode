FROM debian:stable-20240612-slim as base

RUN apt-get update && \
    apt-get install build-essential libpng-dev libtiff-dev -y


WORKDIR /build
COPY . /build/

RUN cd /build/src/jabcode && \
    make
RUN cd /build/src/jabcodeWriter && \
    make
RUN cd /build/src/jabcodeReader && \
    make

FROM debian:stable-20240612-slim

RUN apt-get update && \
    apt-get install libpng-dev libtiff-dev -y

COPY --from=base /build/src/jabcodeWriter/bin/* /usr/bin/
COPY --from=base /build/src/jabcodeReader/bin/* /usr/bin/

WORKDIR /jabcode

CMD ["/bin/bash"]
