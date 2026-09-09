# SPDX-License-Identifier: Apache-2.0
# Ubuntu 24.04; refresh this digest deliberately (Dependabot also checks it).
FROM ubuntu:26.04@sha256:513c074113a871b51a8d16ab445c88779d6452d937a164fb5cc479f32668a41d

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
