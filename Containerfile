FROM ubuntu:22.04

RUN apt-get update -y

RUN apt-get install -y \
    build-essential \
    ruby \
    cmake \
    wget

RUN wget https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz

RUN mkdir -p /bin/crossc/aarch64/
RUN tar xJf arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz \
    -C /bin/crossc/aarch64/

ENV PATH=$PATH:/bin/crossc/aarch64/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin/

RUN apt-get install -y \
    git \
    libncurses-dev \
    flex \
    bison \
    qemu-system-arm \
    libssh-dev \
    bc \
    sudo \
    cpio \
    gzip

RUN apt-get install -y \
    file \
    rsync
RUN apt install -y \
    valgrind \
    netcat \
    sshpass \
    psmisc

#RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && \
#    locale-gen
#ENV LANG en_US.UTF-8
#ENV LANGUAGE en_US:en
#ENV LC_ALL en_US.UTF-8
