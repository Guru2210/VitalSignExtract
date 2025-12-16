#!/bin/bash
# Setup script for Vital Sign Extraction System on Raspberry Pi

set -e  # Exit on error

echo "=== Vital Sign Extraction System - Setup Script ==="
echo ""

# Check if running on Raspberry Pi
if [ ! -f /proc/device-tree/model ]; then
    echo "Warning: This doesn't appear to be a Raspberry Pi"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Update system
echo "Step 1: Updating system packages..."
sudo apt update && sudo apt upgrade -y

# Install build tools
echo "Step 2: Installing build tools..."
sudo apt install -y build-essential cmake pkg-config git

# Install OpenCV
echo "Step 3: Installing OpenCV..."
sudo apt install -y libopencv-dev python3-opencv

# Install Tesseract OCR
echo "Step 4: Installing Tesseract OCR..."
sudo apt install -y tesseract-ocr libtesseract-dev

# Install PostgreSQL client libraries
echo "Step 5: Installing PostgreSQL client libraries..."
sudo apt install -y libpq-dev postgresql-client

# Optional: Install PostgreSQL server locally
read -p "Do you want to install PostgreSQL server locally? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Installing PostgreSQL server..."
    sudo apt install -y postgresql postgresql-contrib
    
    echo "Setting up database..."
    sudo -u postgres psql -c "CREATE DATABASE vital_signs_db;" || echo "Database may already exist"
    sudo -u postgres psql -c "CREATE USER vitalsign_user WITH ENCRYPTED PASSWORD 'changeme';" || echo "User may already exist"
    sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE vital_signs_db TO vitalsign_user;"
    
    echo "Creating database schema..."
    sudo -u postgres psql -d vital_signs_db -f scripts/schema.sql
    
    echo "PostgreSQL setup complete!"
fi

# Check compiler versions
echo ""
echo "Step 6: Verifying compiler installation..."
gcc --version | head -n 1
g++ --version | head -n 1

# Set up cross-compilation environment for ARM (if needed)
if [[ $(uname -m) == "armv7l" ]] || [[ $(uname -m) == "aarch64" ]]; then
    echo ""
    echo "Step 7: Setting up ARM compilation environment..."
    export CC=/usr/bin/gcc
    export CXX=/usr/bin/g++
    echo "export CC=/usr/bin/gcc" >> ~/.bashrc
    echo "export CXX=/usr/bin/g++" >> ~/.bashrc
fi

# Create necessary directories
echo ""
echo "Step 8: Creating necessary directories..."
mkdir -p logs
mkdir -p build

# Verify dependencies
echo ""
echo "Step 9: Verifying dependencies..."
pkg-config --modversion opencv4 || echo "Warning: OpenCV not found via pkg-config"
pkg-config --modversion tesseract || echo "Warning: Tesseract not found via pkg-config"
pkg-config --modversion libpq || echo "Warning: libpq not found via pkg-config"

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Next steps:"
echo "1. Update config/config.json with your settings"
echo "2. Run 'make clean' to clean any old builds"
echo "3. Run 'make' to build the application"
echo "4. Run './build/app' to start the application"
echo ""
echo "For Raspberry Pi 4 (ARMv7l), use:"
echo "  APP_CUSTOM=1 TARGET_LINUX_ARMV7=1 USE_FULL_TFLITE=1 make -j"
echo ""
