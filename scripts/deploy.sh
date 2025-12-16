#!/bin/bash
# Deployment script for Vital Sign Extraction System

set -e

echo "=== Vital Sign Extraction System - Deployment Script ==="
echo ""

# Configuration
INSTALL_DIR="/home/pi/VitalSignExtract"
SERVICE_NAME="vitalsign"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run with sudo"
    exit 1
fi

# Stop service if running
echo "Step 1: Stopping existing service (if running)..."
systemctl stop $SERVICE_NAME || true

# Build the application
echo "Step 2: Building application..."
cd "$(dirname "$0")/.."

# Clean previous build
make clean

# Build for Raspberry Pi 4
if [[ $(uname -m) == "armv7l" ]]; then
    echo "Building for Raspberry Pi 4 (ARMv7l)..."
    APP_CUSTOM=1 TARGET_LINUX_ARMV7=1 USE_FULL_TFLITE=1 make -j$(nproc)
else
    echo "Building for current architecture..."
    make -j$(nproc)
fi

# Verify build
if [ ! -f "build/app" ]; then
    echo "Error: Build failed, executable not found"
    exit 1
fi

echo "Build successful!"

# Install systemd service
echo "Step 3: Installing systemd service..."
cp scripts/vitalsign.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable $SERVICE_NAME

# Create logs directory
mkdir -p logs
chown pi:pi logs

# Set permissions
echo "Step 4: Setting permissions..."
chown -R pi:pi .

# Start service
echo "Step 5: Starting service..."
systemctl start $SERVICE_NAME

# Check status
sleep 2
systemctl status $SERVICE_NAME --no-pager

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "Service commands:"
echo "  Start:   sudo systemctl start $SERVICE_NAME"
echo "  Stop:    sudo systemctl stop $SERVICE_NAME"
echo "  Restart: sudo systemctl restart $SERVICE_NAME"
echo "  Status:  sudo systemctl status $SERVICE_NAME"
echo "  Logs:    sudo journalctl -u $SERVICE_NAME -f"
echo ""
