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

# Run the container and MOUNT your source files
docker run --rm -it \
  -p 5000:5000 \
  -v "$(pwd)/server.py:/app/server.py" \
  -v "$(pwd)/lake_info.json:/app/lake_info.json" \
  -v "$(pwd)/website:/app/website" \
  -e SSL_CERT=/path/in/container/fullchain.pem \
  -e SSL_KEY=/path/in/container/privkey.pem \
  -e USE_SSL \
  "$IMAGE_NAME"
