FROM ubuntu:22.04

RUN apt-get update -y

RUN apt-get install -y \
    build-essential \
    ruby \
    cmake

#RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && \
#    locale-gen
#ENV LANG en_US.UTF-8
#ENV LANGUAGE en_US:en
#ENV LC_ALL en_US.UTF-8
