# syntax=docker/dockerfile:1

FROM ubuntu:22.04
WORKDIR /home/yrd/
COPY ./mk.sh .
RUN apt-get -y update && apt-get -y upgrade
RUN echo 12 > tmp.txt

# install dev tools
RUN apt-get -y install git build-essential
RUN chmod +x /home/yrd/mk.sh
RUN /home/yrd/mk.sh

# sleep forever.
CMD ["sleep", "infinity"]
