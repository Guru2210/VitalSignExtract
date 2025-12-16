-- Database schema for Vital Sign Extraction System
-- PostgreSQL Database

-- Create database (run as postgres superuser)
-- CREATE DATABASE vital_signs_db;
-- CREATE USER vitalsign_user WITH ENCRYPTED PASSWORD 'changeme';
-- GRANT ALL PRIVILEGES ON DATABASE vital_signs_db TO vitalsign_user;

-- Connect to the database
-- \c vital_signs_db

-- Create vital signs table
CREATE TABLE IF NOT EXISTS vital_signs (
    id SERIAL PRIMARY KEY,
    timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    hr VARCHAR(10),
    spo2 VARCHAR(10),
    abp VARCHAR(20),
    ecg_classification VARCHAR(50),
    ecg_confidence REAL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Create indexes for better query performance
CREATE INDEX IF NOT EXISTS idx_vital_signs_timestamp ON vital_signs(timestamp);
CREATE INDEX IF NOT EXISTS idx_vital_signs_created_at ON vital_signs(created_at);

-- Create a view for recent vital signs
CREATE OR REPLACE VIEW recent_vital_signs AS
SELECT 
    id,
    timestamp,
    hr,
    spo2,
    abp,
    ecg_classification,
    ecg_confidence,
    created_at
FROM vital_signs
ORDER BY timestamp DESC
LIMIT 1000;

-- Grant permissions to user
GRANT ALL PRIVILEGES ON TABLE vital_signs TO vitalsign_user;
GRANT USAGE, SELECT ON SEQUENCE vital_signs_id_seq TO vitalsign_user;
GRANT SELECT ON recent_vital_signs TO vitalsign_user;

-- Create a function to clean old data (optional, for maintenance)
CREATE OR REPLACE FUNCTION cleanup_old_vital_signs(days_to_keep INTEGER DEFAULT 30)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM vital_signs
    WHERE created_at < NOW() - INTERVAL '1 day' * days_to_keep;
    
    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- Example: Call cleanup function to remove data older than 30 days
-- SELECT cleanup_old_vital_signs(30);

COMMENT ON TABLE vital_signs IS 'Stores vital sign measurements extracted from patient monitoring system';
COMMENT ON COLUMN vital_signs.timestamp IS 'Timestamp when the vital signs were captured';
COMMENT ON COLUMN vital_signs.hr IS 'Heart Rate';
COMMENT ON COLUMN vital_signs.spo2 IS 'Blood Oxygen Saturation (SpO2)';
COMMENT ON COLUMN vital_signs.abp IS 'Arterial Blood Pressure (format: systolic/diastolic)';
COMMENT ON COLUMN vital_signs.ecg_classification IS 'ECG classification result from ML model';
COMMENT ON COLUMN vital_signs.ecg_confidence IS 'Confidence score of ECG classification';
