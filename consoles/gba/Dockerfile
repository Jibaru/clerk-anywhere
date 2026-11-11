FROM devkitpro/devkitarm:latest

ARG BUTANO_VERSION=21.10.0

RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 python-is-python3 git make ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && git clone --depth 1 --branch ${BUTANO_VERSION} https://github.com/GValiente/butano.git /opt/butano

ENV LIBBUTANO=/opt/butano/butano
WORKDIR /work
