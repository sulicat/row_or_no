#!/usr/bin/env bash
set -e

IMAGE_NAME=roworno

# Build the image (only builds Python + Flask)
docker build -t "$IMAGE_NAME" .

# Run the container and MOUNT your source files
docker run --rm -it \
  -p 5000:5000 \
  -v "$(pwd)/server.py:/app/server.py" \
  -v "$(pwd)/lake_info.json:/app/lake_info.json" \
  -v "$(pwd)/website:/app/website" \
  "$IMAGE_NAME"
