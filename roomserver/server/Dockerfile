FROM debian:buster-slim as builder

# Install app dependencies
RUN apt-get update -y --fix-missing && apt-get install -y \
    cmake \
    g++ \
    libcurl4-openssl-dev \
    libjsoncpp-dev

# Create app directory
RUN mkdir -p /usr/src/app
RUN mkdir -p /usr/src/app/build
WORKDIR /usr/src/app/build

# Bundle app source
COPY . /usr/src/app

# build
RUN cmake -DCMAKE_BUILD_TYPE=Release ..
RUN make

# ===== OKAY - FORGET ALL THAT AND START ANEW ===

FROM debian:buster-slim

# THESE GUYS ONLY REQUIRED DURING BUILD
RUN apt-get update -y --fix-missing && apt-get install -y \
    libcurl4 \
    libjsoncpp1 && \
    rm -rf /var/lib/apt/lists/*

# Create app directoryg+
RUN mkdir -p /usr/src/app
RUN mkdir -p /usr/src/app/build
WORKDIR /usr/src/app/build

# Copy the pre-built server from the other image << THIS IS THE MAGIC TRICK
COPY --from=builder /usr/src/app/build/RoomServerExampleServer .

# Execute
ENTRYPOINT ["./RoomServerExampleServer"]
