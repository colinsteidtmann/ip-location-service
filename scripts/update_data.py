import os
import csv
import io
import requests
import psycopg2
from psycopg2 import extras
import time
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Database connection details
DB_HOST = os.getenv("DB_HOST", "db")
DB_NAME = os.getenv("DB_NAME", "ip_locations_db")
DB_USER = os.getenv("DB_USER", "ip_user")
DB_PASSWORD = os.getenv("DB_PASSWORD", "ip_password")
DB_PORT = os.getenv("DB_PORT", "5432")

# CSV URL
CSV_URL = "https://docs.google.com/uc?export=download&id=1jSFgZC37plw90CkioEsKvunaeMKUv_rq"

def get_db_connection():
    """Establishes and returns a PostgreSQL database connection."""
    conn_string = f"host={DB_HOST} port={DB_PORT} dbname={DB_NAME} user={DB_USER} password={DB_PASSWORD}"
    retries = 10
    for i in range(retries):
        try:
            conn = psycopg2.connect(conn_string)
            logger.info("Successfully connected to PostgreSQL.")
            return conn
        except psycopg2.OperationalError as e:
            logger.warning(f"Connection attempt {i+1}/{retries} failed: {e}")
            if i < retries - 1:
                time.sleep(3)
    raise Exception("Failed to connect to PostgreSQL after multiple retries.")

def download_csv_data(url):
    """Downloads CSV data from the given URL and returns it as a string."""
    logger.info(f"Downloading CSV data from {url}...")
    response = requests.get(url, stream=True, timeout=300)  # 5 minute timeout
    response.raise_for_status()
    return response.content.decode('utf-8')

def update_ip_data():
    conn = None
    try:
        conn = get_db_connection()
        conn.autocommit = False # Ensure transactions are used

        csv_data_str = download_csv_data(CSV_URL)
        csv_file_like_object = io.StringIO(csv_data_str)

        # --- Transaction for creating staging table and importing ---
        with conn.cursor() as cur:
            logger.info("Starting transaction for staging table and import...")
            
            # Check if main table exists, if not create it
            cur.execute("""
                CREATE TABLE IF NOT EXISTS ip_locations (
                    start_ip INET NOT NULL,
                    end_ip INET NOT NULL,
                    network_ip INET,
                    city VARCHAR(255),
                    region VARCHAR(255),
                    country VARCHAR(2) NOT NULL,
                    latitude REAL,
                    longitude REAL,
                    postal_code VARCHAR(10),
                    timezone VARCHAR(50)
                );
            """)
            
            cur.execute("DROP TABLE IF EXISTS ip_locations_new;")
            cur.execute("""
                CREATE TABLE ip_locations_new (
                    start_ip INET NOT NULL,
                    end_ip INET NOT NULL,
                    network_ip INET,
                    city VARCHAR(255),
                    region VARCHAR(255),
                    country VARCHAR(2) NOT NULL,
                    latitude REAL,
                    longitude REAL,
                    postal_code VARCHAR(10),
                    timezone VARCHAR(50)
                );
            """)

            csv_file_like_object.readline() # Skip header row

            # Define the columns exactly as they appear in your CSV header:
            csv_columns_order = (
                'start_ip',
                'end_ip',
                'network_ip',
                'city',
                'region',
                'country',
                'latitude',
                'longitude',
                'postal_code',
                'timezone'
            )

            # Use copy_expert with proper CSV format handling
            copy_sql = f"""
                COPY ip_locations_new ({','.join(csv_columns_order)})
                FROM STDIN WITH (
                    FORMAT CSV,
                    DELIMITER ',',
                    QUOTE '"',
                    ESCAPE '"'
                )
            """
            
            cur.copy_expert(copy_sql, csv_file_like_object)
            
            logger.info(f"Copied {cur.rowcount} rows into ip_locations_new.")

            # Create indexes on the new table *before* the swap
            logger.info("Creating indexes...")
            cur.execute("CREATE INDEX idx_ip_locations_new_start_ip ON ip_locations_new (start_ip);")
            cur.execute("CREATE INDEX idx_ip_locations_new_end_ip ON ip_locations_new (end_ip);")
            cur.execute("CREATE INDEX idx_ip_locations_new_ip_range ON ip_locations_new (start_ip, end_ip);")
            
            # Add constraint for data integrity
            cur.execute("ALTER TABLE ip_locations_new ADD CONSTRAINT chk_ip_range CHECK (start_ip <= end_ip);")
            
            conn.commit()
            logger.info("Data imported into staging table and indexes created.")

        # --- Transaction for atomic swap ---
        with conn.cursor() as cur:
            logger.info("Starting transaction for atomic table swap...")
            
            # Check if main table exists before attempting swap
            cur.execute("""
                SELECT EXISTS (
                    SELECT FROM information_schema.tables 
                    WHERE table_name = 'ip_locations'
                );
            """)
            
            if cur.fetchone()[0]:
                # Lock existing table briefly to ensure atomicity during rename
                cur.execute("LOCK TABLE ip_locations IN ACCESS EXCLUSIVE MODE;")
                cur.execute("LOCK TABLE ip_locations_new IN ACCESS EXCLUSIVE MODE;")

                # Rename the old table
                cur.execute("ALTER TABLE ip_locations RENAME TO ip_locations_old;")
                
                # Rename the new table to the active name
                cur.execute("ALTER TABLE ip_locations_new RENAME TO ip_locations;")
                
                # Drop the old table
                cur.execute("DROP TABLE ip_locations_old;")
            else:
                # First time setup - just rename the new table
                cur.execute("ALTER TABLE ip_locations_new RENAME TO ip_locations;")
            
            conn.commit()
            logger.info("Atomic table swap complete.")

    except requests.exceptions.RequestException as e:
        logger.error(f"Error downloading CSV: {e}")
        if conn: conn.rollback()
        raise
    except psycopg2.Error as e:
        logger.error(f"Database error: {e}")
        if conn: conn.rollback()
        if hasattr(e, 'pgerror'):
            logger.error(f"PostgreSQL Error Details: {e.pgerror}")
        raise
    except Exception as e:
        logger.error(f"An unexpected error occurred: {e}")
        if conn: conn.rollback()
        raise
    finally:
        if conn:
            conn.close()
            logger.info("Database connection closed.")

if __name__ == "__main__":
    logger.info("Initiating IP data update script...")
    try:
        update_ip_data()
        logger.info("IP data update script finished successfully.")
    except Exception as e:
        logger.error(f"IP data update script failed: {e}")
        exit(1)