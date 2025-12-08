FROM python:3.10-slim

WORKDIR /app

# Create a venv inside the container
RUN python3 -m venv /app/venv

# Install dependencies inside the venv
RUN /app/venv/bin/pip install --no-cache-dir flask

# Expose port
EXPOSE 5000

# Run server using the venv's python
CMD ["/app/venv/bin/python", "server.py"]
