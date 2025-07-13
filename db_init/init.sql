-- db_init/init.sql
-- Create the schema if it doesn't exist (optional, but good practice)
CREATE SCHEMA IF NOT EXISTS public; -- 'public' is the default schema

-- Set search path to public (ensure tables are created in and found in public)
SET search_path TO public;

-- Create the main ip_locations table with the correct structure
CREATE TABLE IF NOT EXISTS ip_locations (
    start_ip INET NOT NULL,
    end_ip INET NOT NULL,
    network_ip INET,
    city VARCHAR(255),
    region VARCHAR(255),
    country VARCHAR(2) NOT NULL,  -- ISO 2-letter country code
    latitude REAL,
    longitude REAL,
    postal_code VARCHAR(10),
    timezone VARCHAR(50)
);

-- Create indexes for efficient IP range lookups
-- Use B-tree indexes since GIST doesn't have default operator class for INET
CREATE INDEX IF NOT EXISTS idx_ip_locations_start_ip ON ip_locations (start_ip);
CREATE INDEX IF NOT EXISTS idx_ip_locations_end_ip ON ip_locations (end_ip);
CREATE INDEX IF NOT EXISTS idx_ip_locations_ip_range ON ip_locations (start_ip, end_ip);

-- Add constraint for data integrity
ALTER TABLE ip_locations ADD CONSTRAINT IF NOT EXISTS chk_ip_range CHECK (start_ip <= end_ip);

-- Create index on country for fast country-based queries
CREATE INDEX IF NOT EXISTS idx_ip_locations_country ON ip_locations (country);

-- Optional: Create a view for common queries
CREATE OR REPLACE VIEW ip_location_summary AS
SELECT 
    country,
    COUNT(*) as range_count,
    COUNT(DISTINCT city) as unique_cities,
    COUNT(DISTINCT region) as unique_regions
FROM ip_locations 
GROUP BY country;

-- Grant permissions to the application user
GRANT SELECT, INSERT, UPDATE, DELETE ON ip_locations TO ip_user;
GRANT USAGE ON SCHEMA public TO ip_user;

-- Optional: Create a function for IP lookup (can be used by the API)
CREATE OR REPLACE FUNCTION lookup_ip_location(target_ip INET)
RETURNS TABLE(
    country VARCHAR(2),
    city VARCHAR(255),
    region VARCHAR(255),
    latitude REAL,
    longitude REAL,
    postal_code VARCHAR(10),
    timezone VARCHAR(50)
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        il.country,
        il.city,
        il.region,
        il.latitude,
        il.longitude,
        il.postal_code,
        il.timezone
    FROM ip_locations il
    WHERE target_ip >= il.start_ip AND target_ip <= il.end_ip
    ORDER BY il.start_ip
    LIMIT 1;
END;
$$ LANGUAGE plpgsql;

-- Grant execute permission on the function
GRANT EXECUTE ON FUNCTION lookup_ip_location(INET) TO ip_user;

-- Log successful initialization
SELECT 'ip_locations table and indexes created successfully!' AS message;