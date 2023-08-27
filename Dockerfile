FROM ubuntu:latest
RUN apt update
RUN apt install -y g++ make iproute2
COPY . /project/
WORKDIR /project/
RUN make
ENTRYPOINT ./build/wakeonlan