# Vital Sign Extraction System

A production-ready application for Raspberry Pi that extracts vital signs (Heart Rate, SpO2, Arterial Blood Pressure) from patient monitoring displays using OpenCV OCR and performs ECG classification using TensorFlow Lite.

## Features

- **Real-time Vital Sign Extraction**: OCR-based extraction of HR, SpO2, and ABP from patient monitors
- **ECG Classification**: TensorFlow Lite model for heart health assessment
- **Database Integration**: PostgreSQL storage for historical data
- **Comprehensive Logging**: Multi-level logging with file rotation
- **Configuration Management**: JSON-based configuration system
- **Error Recovery**: Automatic camera reconnection and database retry logic
- **Production Ready**: Systemd service, graceful shutdown, and monitoring support

## Architecture

```
VitalSignExtract/
├── src/
│   ├── config/          # Configuration management
│   ├── database/        # PostgreSQL integration
│   ├── ocr/             # OCR processing (future)
│   ├── ml/              # ML inference (future)
│   └── utils/           # Logging and utilities
├── config/              # Configuration files
├── scripts/             # Setup and deployment scripts
├── logs/                # Application logs
├── edge-impulse-sdk/    # Edge Impulse ML SDK
├── tflite-model/        # TensorFlow Lite model
└── main.cpp             # Main application
```

## Requirements

### Hardware
- Raspberry Pi 4 (4GB+ RAM recommended)
- Camera Module or USB camera
- 32GB+ SD card
- Reliable power supply

### Software
- Raspberry Pi OS (64-bit)
- C++17 compiler
- OpenCV 4.x
- Tesseract OCR
- PostgreSQL (optional, for database storage)

## Quick Start

### 1. Clone Repository
```bash
git clone <repository-url>
cd VitalSignExtract
```

### 2. Run Setup Script
```bash
chmod +x scripts/setup.sh
sudo scripts/setup.sh
```

This will:
- Install all dependencies
- Set up PostgreSQL (optional)
- Create necessary directories
- Configure the environment

### 3. Configure Application
Edit `config/config.json` to match your setup:
```json
{
  "video": {
    "source_type": "camera",  // or "file"
    "camera_index": 0,
    "source_path": "/path/to/video.mp4"
  },
  "database": {
    "enabled": true,
    "host": "localhost",
    "password": "your_password"
  }
}
```

### 4. Build Application
```bash
# For Raspberry Pi 4 (ARMv7l)
APP_CUSTOM=1 TARGET_LINUX_ARMV7=1 USE_FULL_TFLITE=1 make -j

# For other systems
make
```

### 5. Run Application
```bash
# Direct execution
./build/app

# With custom config
./build/app /path/to/config.json
```

## Deployment

### Manual Deployment
```bash
chmod +x scripts/deploy.sh
sudo scripts/deploy.sh
```

### Service Management
```bash
# Start service
sudo systemctl start vitalsign

# Stop service
sudo systemctl stop vitalsign

# Restart service
sudo systemctl restart vitalsign

# View status
sudo systemctl status vitalsign

# View logs
sudo journalctl -u vitalsign -f
```

## Configuration

### Video Source
```json
"video": {
  "source_type": "camera",      // "camera" or "file"
  "camera_index": 0,             // Camera device index
  "source_path": "/path/to/video.mp4",
  "processing_interval": 300,    // Process every N frames
  "reconnect_attempts": 5,       // Camera reconnection attempts
  "reconnect_delay_ms": 2000     // Delay between reconnection attempts
}
```

### Database
```json
"database": {
  "enabled": true,
  "type": "postgresql",
  "host": "localhost",
  "port": 5432,
  "database": "vital_signs_db",
  "user": "vitalsign_user",
  "password": "changeme",
  "connection_pool_size": 5,
  "retry_attempts": 3
}
```

### Logging
```json
"logging": {
  "level": "info",               // debug, info, warn, error, critical
  "console_enabled": true,
  "file_enabled": true,
  "file_path": "logs/vitalsign.log",
  "max_file_size_mb": 10,
  "max_files": 5
}
```

## Database Setup

### Create Database
```bash
sudo -u postgres psql
```

```sql
CREATE DATABASE vital_signs_db;
CREATE USER vitalsign_user WITH ENCRYPTED PASSWORD 'changeme';
GRANT ALL PRIVILEGES ON DATABASE vital_signs_db TO vitalsign_user;
\q
```

### Initialize Schema
```bash
sudo -u postgres psql -d vital_signs_db -f scripts/schema.sql
```

## Output

### Console Output
```
Time: 2025-12-16 17:30:45 | HR: 72 | SpO₂: 98 | ABP: 120/80 | ECG: normal (0.95)
```

### CSV Output
Data is saved to `live_vital_signs_output.csv`:
```csv
Time,HR,SpO2,ABP,ECG_Classification,ECG_Confidence
2025-12-16 17:30:45,72,98,120/80,normal,0.95
```

### Database Storage
Data is automatically stored in PostgreSQL when enabled.

## Monitoring

### View Logs
```bash
# Application logs
tail -f logs/vitalsign.log

# System logs
sudo journalctl -u vitalsign -f
```

### Health Check
```bash
# Check service status
sudo systemctl status vitalsign

# Check database connection
psql -h localhost -U vitalsign_user -d vital_signs_db -c "SELECT COUNT(*) FROM vital_signs;"
```

## Troubleshooting

### Camera Not Opening
1. Check camera connection
2. Verify camera index in config
3. Check permissions: `sudo usermod -a -G video pi`
4. Test camera: `raspistill -o test.jpg`

### Database Connection Failed
1. Verify PostgreSQL is running: `sudo systemctl status postgresql`
2. Check credentials in config
3. Test connection: `psql -h localhost -U vitalsign_user -d vital_signs_db`
4. Check firewall settings

### OCR Not Detecting Text
1. Verify Tesseract installation: `tesseract --version`
2. Check OCR confidence threshold in config
3. Ensure proper lighting and camera focus
4. Test with debug mode enabled

### Build Errors
1. Clean build: `make clean`
2. Verify dependencies: `pkg-config --modversion opencv4 tesseract libpq`
3. Check compiler version: `g++ --version`
4. Review error messages in build output

## Performance Tuning

### Optimize Processing Interval
Adjust `processing_interval` in config based on your needs:
- Higher value = Lower CPU usage, slower updates
- Lower value = Higher CPU usage, faster updates

### Database Performance
- Enable connection pooling
- Use prepared statements (already implemented)
- Regular database maintenance: `VACUUM ANALYZE vital_signs;`

### Memory Management
- Monitor with: `htop` or `free -h`
- Adjust log file size and rotation
- Consider reducing ML model size if needed

## Development

### Build for Development
```bash
# Enable debug mode in config.json
"application": {
  "debug_mode": true
}

# Build
make clean && make
```

### Adding New Features
1. Create new classes in appropriate `src/` subdirectory
2. Add source files to `Makefile`
3. Update configuration schema if needed
4. Add tests (recommended)
5. Update documentation

## License

See LICENSE file for details.

## Support

For issues and questions:
1. Check logs: `logs/vitalsign.log`
2. Review configuration
3. Consult troubleshooting section
4. Check system resources

## Changelog

### Version 1.0.0 (Production Ready)
- ✅ Configuration management system
- ✅ Comprehensive logging with rotation
- ✅ PostgreSQL database integration
- ✅ Error handling and recovery
- ✅ Graceful shutdown
- ✅ Systemd service support
- ✅ Automated deployment scripts
- ✅ Camera reconnection logic
- ✅ Database retry mechanism

### Previous Version (Prototype)
- Basic OCR extraction
- TFLite ML inference
- CSV output only
- Hardcoded configuration
