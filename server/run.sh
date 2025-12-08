#!/usr/bin/env bash
set -e

IMAGE_NAME=roworno

# Build the image (only builds Python + Flask)
docker build -t "$IMAGE_NAME" .

USE_SSL=1
if [[ "$1" == "--no-ssl" ]]; then
    USE_SSL=0
fi

export USE_SSL

export HOST_CERT=${SSL_CERT}
export HOST_KEY=${SSL_KEY}

# Run the container and MOUNT your source files
docker run --rm -it \
  -p 5000:5000 \
  -v "$(pwd)/server.py:/app/server.py" \
  -v "$(pwd)/lake_info.json:/app/lake_info.json" \
  -v "$(pwd)/website:/app/website" \
  -v "$HOST_CERT:/root/fullchain.pem:ro" \
  -v "$HOST_KEY:/root/privkey.pem:ro" \
  -e USE_SSL \
  -e SSL_CERT=/root/fullchain.pem \
  -e SSL_KEY=/root/privkey.pem \
  "$IMAGE_NAME"
