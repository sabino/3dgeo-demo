# SPDX-License-Identifier: GPL-3.0-or-later
# Ubuntu 24.04; refresh this digest deliberately (Dependabot also checks it).
FROM ubuntu:24.04@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates software-properties-common \
    && add-apt-repository -y ppa:dciabrin/ngdevkit \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential git pkg-config python3 python3-pil \
        ngdevkit ngdevkit-gngeo mame xvfb xauth \
    && apt-get clean

# Packages are upstream nightlies, not an immutable toolchain lock. The CI
# artifact records exact installed versions to make regressions diagnosable.
WORKDIR /workspace
ENV PATH="/usr/games:${PATH}"
CMD ["bash", "ci/run.sh"]
