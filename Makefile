BUILD_IMG=buildroot_build_env

UID=$(shell id -u)
GID=$(shell id -g)

all:
	podman run -it \
	--userns=keep-id \
	-w /proj \
	-v ${PWD}:/proj \
	--device=/dev/net/tun:/dev/net/tun \
	${BUILD_IMG} \
	./build.sh
.PHONY: all

shell:
	podman run -it --rm \
	--userns=keep-id \
	-w /proj \
	-v ${PWD}:/proj:rw,Z \
	-v /tmp:/tmp \
	--device=/dev/net/tun:/dev/net/tun \
	${BUILD_IMG} \
	/bin/bash

.PHONY: all


build_env:
	podman build \
	-f Containerfile \
	-t $(BUILD_IMG)

