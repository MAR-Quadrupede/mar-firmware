FROM python:3.11-slim

WORKDIR /workspace

# Install PlatformIO for firmware compilation and testing
RUN pip install --no-cache-dir platformio

COPY . .

# Default command: build the firmware using PlatformIO
CMD ["pio", "run"]
