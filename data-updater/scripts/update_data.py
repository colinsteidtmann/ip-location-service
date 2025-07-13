import os
import csv
import io
import requests
import psycopg2
from psycopg2 import extras
import time
import logging
import schedule
from datetime import datetime, timedelta

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

DB_HOST = os.getenv("DB_HOST", "db")
DB_NAME = os.getenv("DB_NAME", "ip_locations_db")
DB_USER = os.getenv("DB_USER", "ip_user")
DB_PASSWORD = os.getenv("DB_PASSWORD", "ip_password")
DB_PORT = os.getenv("DB_PORT", "5432")

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
    response = requests.get(url, stream=True, timeout=300)
    response.raise_for_status()
    return response.content.decode('utf-8')

def update_ip_data():
    conn = None
    try:
        conn = get_db_connection()
        conn.autocommit = False # for transactions

        csv_data_str = download_csv_data(CSV_URL)
        csv_file_like_object = io.StringIO(csv_data_str)

        with conn.cursor() as cur:
            logger.info("Starting transaction for staging table and import...")
            
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

            csv_file_like_object.readline()#skip header line

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

            logger.info("Creating indexes...")
            cur.execute("CREATE INDEX ON ip_locations_new (start_ip) WHERE start_ip IS NOT NULL;")
            cur.execute("CREATE INDEX ON ip_locations_new (end_ip) WHERE end_ip IS NOT NULL;")
            cur.execute("CREATE INDEX ON ip_locations_new (start_ip, end_ip) WHERE start_ip IS NOT NULL AND end_ip IS NOT NULL;")
        
            cur.execute("ALTER TABLE ip_locations_new ADD CONSTRAINT chk_ip_range CHECK (start_ip <= end_ip);")
            
            conn.commit()
            logger.info("Data imported into staging table and indexes created.")

        # atomic swap
        with conn.cursor() as cur:
            logger.info("Starting transaction for atomic table swap...")
            
            cur.execute("""
                SELECT EXISTS (
                    SELECT FROM information_schema.tables 
                    WHERE table_name = 'ip_locations'
                );
            """)
            
            if cur.fetchone()[0]:
                cur.execute("LOCK TABLE ip_locations IN ACCESS EXCLUSIVE MODE;")
                cur.execute("LOCK TABLE ip_locations_new IN ACCESS EXCLUSIVE MODE;")

                cur.execute("ALTER TABLE ip_locations RENAME TO ip_locations_old;")
                
                cur.execute("ALTER TABLE ip_locations_new RENAME TO ip_locations;")
                
                cur.execute("DROP TABLE ip_locations_old;")
            else:
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

class DataUpdateScheduler:
    def __init__(self):
        self.last_update = None
        self.update_in_progress = False

    def schedule_updates(self):
        """Schedule daily updates at 2 AM"""
        schedule.every().day.at("02:00").do(self.safe_update)
        logger.info("Scheduled daily updates at 2:00 AM")
        
        while True:
            schedule.run_pending()
            time.sleep(60)

    def safe_update(self):
        """Safely execute the update with proper error handling and locking"""
        if self.update_in_progress:
            logger.warning("Update already in progress, skipping...")
            return

        try:
            self.update_in_progress = True
            logger.info("Starting scheduled IP data update...")
            update_ip_data()
            self.last_update = datetime.now()
            logger.info("Data update completed successfully")
        except Exception as e:
            logger.error(f"Data update failed: {e}")
        finally:
            self.update_in_progress = False

    def run_manual_update(self):
        """Run a manual update"""
        logger.info("Running manual IP data update...")
        self.safe_update()

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == "--schedule":
        #schedule updates
        logger.info("Starting IP data update scheduler...")
        scheduler = DataUpdateScheduler()
        try:
            scheduler.schedule_updates()
        except KeyboardInterrupt:
            logger.info("Scheduler stopped by user")
        except Exception as e:
            logger.error(f"Scheduler failed: {e}")
            exit(1)
    elif len(sys.argv) > 1 and sys.argv[1] == "--run-now":
        #run immediate update
        logger.info("Running immediate IP data update...")
        scheduler = DataUpdateScheduler()
        scheduler.run_manual_update()
    else:
        #default behavior is to run the update script once
        logger.info("Initiating IP data update script...")
        try:
            update_ip_data()
            logger.info("IP data update script finished successfully.")
        except Exception as e:
            logger.error(f"IP data update script failed: {e}")
            exit(1)