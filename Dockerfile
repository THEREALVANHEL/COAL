FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && \
    apt-get install -y build-essential cmake git libssl-dev libsasl2-dev wget pkg-config

# Install MongoDB C driver
RUN wget https://github.com/mongodb/mongo-c-driver/releases/download/1.24.4/mongo-c-driver-1.24.4.tar.gz && \
    tar -xzf mongo-c-driver-1.24.4.tar.gz && \
    cd mongo-c-driver-1.24.4 && \
    mkdir -p build && cd build && \
    cmake .. -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF && \
    cmake --build . --target install && \
    cd ../.. && rm -rf mongo-c-driver-1.24.4*

# Install MongoDB C++ driver
RUN wget https://github.com/mongodb/mongo-cxx-driver/releases/download/r3.9.0/mongo-cxx-driver-r3.9.0.tar.gz && \
    tar -xzf mongo-cxx-driver-r3.9.0.tar.gz && \
    cd mongo-cxx-driver-r3.9.0/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_PREFIX_PATH=/usr/local && \
    cmake --build . --target install && \
    cd ../.. && rm -rf mongo-cxx-driver-r3.9.0*

# Install DPP
RUN git clone https://github.com/brainboxdotcc/DPP.git && \
    cd DPP && \
    cmake . -Bbuild && \
    cmake --build build && \
    cmake --install build && \
    cd .. && rm -rf DPP

# Copy your bot code
WORKDIR /app
COPY . .

# Build your bot
RUN mkdir -p build && cd build && cmake .. -DCMAKE_PREFIX_PATH="/usr/local" && make

# Set environment variables (Render will override these)
ENV DISCORD_TOKEN=changeme
ENV MONGODB_URI=changeme

# Start your bot
CMD ["build/discord_cpp_bot"]
