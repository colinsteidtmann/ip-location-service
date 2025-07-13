# IP Location Service

A C++ API service that returns geographical location information for IP addresses.

## Quick Start

```bash
#clone the repo
git clone <repository-url>
cd ip-location-service

#start all services with Docker Compose
docker-compose up

#test the API
curl localhost:8080/ip-location?ip=108.160.94.90
```

Expected response:
```json
{"timezone":"America/Toronto","postal_code":"N5A","longitude":-80.9497199999999992315,"region":"Ontario","latitude":43.3667900000000017258,"city":"Stratford","country":"CA","ip":"108.160.94.90"}
```

## Features

- C++ Backend
- Redis Caching
- Rate Limiting (based on users' IP)
- Atomic database swaps for daily data updates
- Database and Redis Health Check endpoint
- Full containerization with Docker Compose
- Stateless design for horizontal scaling

## Architecture

The service consists of several components working together:

- **API Service**: C++ application using the Crow framework that handles HTTP requests
- **PostgreSQL Database**: Stores the IP location data with optimized indexes for fast lookups  
- **Redis Cache**: Caches frequently requested IP locations to reduce database load
- **Data Updater**: Python service that downloads fresh data daily and updates the database

The API service connects to both PostgreSQL (for data) and Redis (for caching). The data updater runs on a schedule to keep the IP location data current.

## API Documentation

### Endpoints

#### Get IP Location
```http
GET /ip-location?ip={ip_address}
```

**Parameters:**
- `ip` (required): IPv4 or IPv6 address to lookup

**Response:**
```json
{"timezone":"America/Toronto","postal_code":"N5A","longitude":-80.9497199999999992315,"region":"Ontario","latitude":43.3667900000000017258,"city":"Stratford","country":"CA","ip":"108.160.94.90"}
```

**Error Responses:**
```json
{"error":"Invalid IP address format","code":"INVALID_IP"}
```

#### Health Check
```http
GET /health
```

Returns service health status and dependency checks.

Example response:
```json
{"cache":{"status":"healthy"},"database":{"status":"healthy"},"timestamp":1752460233,"status":"healthy"}
```

#### Metrics
```http
GET /metrics
```

Returns performance metrics and statistics.

*Caching metrics coming soon...*

Example response:
```json
{"uptime":184166,"redis_connected":true,"redis_healthy":true,"database_healthy":true}
```

## Development Setup

### Prerequisites

- Docker & Docker Compose
- Git

### Local Development

1. **Start all services:**
   ```bash
   docker-compose up -d
   ```
   
   The API will automatically build and start. You can check the logs with:
   ```bash
   docker-compose logs api
   ```

2. **Test the API:**
   ```bash
   curl localhost:8080/health
   curl localhost:8080/ip-location?ip=8.8.8.8
   ```

3. **For development work:**
   ```bash
   #access the API container
   docker-compose exec api bash
   
   #run tests
   cd /home/appuser/app/api
   ./run_tests.sh
   ```

### Project Structure

```
├── api/                    # C++ API service
│   ├── src/
│   │   ├── main.cpp       # Application entry point
│   │   ├── config/        # Configuration management
│   │   ├── database/      # Database connection pooling
│   │   ├── handlers/      # HTTP request handlers
│   │   └── utils/         # Utilities (logging, validation, etc.)
│   ├── tests/             # Unit tests
│   ├── Dockerfile         # API service container
│   └── CMakeLists.txt     # Build configuration
├── data-updater/          # Python data updater service
│   ├── scripts/
│   │   └── update_data.py # Data download and update logic
│   ├── Dockerfile         # Updater service container
│   └── requirements.txt   # Python dependencies
├── db_init/               # Database initialization scripts
└── docker-compose.yml     # Multi-service configuration
```

### Database Schema

```sql
CREATE TABLE ip_locations (
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

CREATE INDEX idx_ip_locations_range ON ip_locations USING gist (inet_range(start_ip, end_ip));
CREATE INDEX idx_ip_locations_start_ip ON ip_locations (start_ip);
```

## AWS Architecture
[IP Location Service - AWS Architecture](https://docs.google.com/document/d/1LY2cClGmse9SVIXYN2oBj4R4xG-z-78mtTgAQ4QyZxg/edit?usp=sharing)

# IP Location Service - AWS Architecture

## Overview
A scalable IP API service designed to handle 1000 requests per second with daily data updates and zero downtime.

---

## Architecture Components

### 1. API Service Layer
**Amazon ECS Fargate** - Containerized C++ API service

**Why this choice:** Fargate eliminates server management while giving us full control over the containerized C++ application. The multi-AZ deployment ensures high availability, and auto-scaling handles traffic spikes efficiently.

### 2. Database Layer
**Amazon RDS PostgreSQL** - Primary database with read replicas
* Primary: `db.r6g.xlarge` (4 vCPU, 32GB RAM)
* Read replicas: 2x `db.r6g.large` (2 vCPU, 16GB RAM each)
* Storage: 1TB GP3 SSD with 16,000 IOPS
* Multi-AZ enabled for automatic failover
* Daily backups with 7-day retention

**Why this choice:** PostgreSQL's `INET` data types and `GiST` indexes are perfect for IP range queries. Read replicas offload traffic from the primary database, and Multi-AZ provides automatic failover for high availability.

**Why not DynamoDB or MongoDB?**
* **DynamoDB:** IP geolocation requires range queries (finding which IP range contains a specific IP). DynamoDB doesn't support range queries efficiently - you'd need to scan the entire table or use complex workarounds with multiple hash keys.
* **MongoDB:** While MongoDB can handle IP ranges, it lacks PostgreSQL's native `INET` data types and specialized `GiST` indexes. The query performance for IP range lookups would be significantly slower, and you'd lose the benefits of proven SQL tooling and monitoring.

### 3. Caching Layer
**Amazon ElastiCache for Redis** - In-memory caching
* 2 nodes with replication (`cache.r6g.large` - 2 vCPU, 13GB RAM each)
* Multi-AZ deployment for reliability
* 12-hour TTL on cached lookups
* LRU eviction policy

**Why this choice:** Redis significantly reduces database load for frequently queried IPs. The 12-hour TTL ensures users get updated data within half a day after daily updates.

### 4. Load Balancing & CDN
**Amazon CloudFront** - Global CDN
* 1-hour cache on IP lookup responses
* Global edge locations for low latency
* Forwards only IP parameters to backend
**Application Load Balancer** - Regional load balancer
* Distributes traffic across ECS tasks
* SSL/TLS termination
* Health check integration

**Why this choice:** CloudFront reduces latency globally and offloads repeated requests. ALB provides intelligent routing and integrates well with ECS auto-scaling.

### 5. Data Update Pipeline
**AWS Lambda** - Serverless data updater
* Python 3.11 runtime, 1GB memory, 15-minute timeout
* Triggered daily at 12 AM UTC by Amazon EventBridge
* Zero-downtime updates using atomic table swaps
* Runs in VPC for secure database access

**Why this choice:** For a once-daily script that runs quickly, Lambda is the most cost-effective option. The 15-minute timeout is sufficient for data processing, and the serverless nature eliminates infrastructure management.

---

## Monthly Cost Estimates (US East)

### ECS Fargate (API Service)
* 6 tasks × 2 vCPU × 4GB RAM × 730 hours = $432.48/month
  * Price per vCPU per hour: $0.04048
  * Price per GB per hour: $0.004445
* Auto-scaling overhead (estimated 30% additional capacity) = $129.74/month
* **Total: $562.22/month**
* *Source: [AWS ECS Fargate Pricing](https://aws.amazon.com/fargate/pricing/)*

### RDS PostgreSQL
* Primary `db.r6g.xlarge`: $328.50/month
  * $0.4500 per hour for multi-az deployment
* Read replicas 2x `db.r6g.large`: $328.50/month
  * $0.2250 per hour for single-az deployment
* Storage 1TB GP3 + IOPS: $80.00/month
  * GP3 storage in US East (N. Virginia) costs $0.08 per GB-month
  * Baseline 3,000 IOPS and 125 MB/s throughput included for free.
* **Total: $737.00/month**
* *Source: [AWS RDS Pricing](https://aws.amazon.com/rds/postgresql/pricing/)*

### ElastiCache Redis
* 2 nodes `cache.r6g.large`: $328.50/month
  * $0.2250 per hour
* **Total: $328.50/month**
* *Source: [AWS ElastiCache Pricing](https://aws.amazon.com/elasticache/pricing/)*

### CloudFront
* 1000 RPS with 50% cache hit rate: **$1,304.00/month (max)**
  * Total Requests per Month: 1000 requests/second * 3600 seconds/hour * 730 hours/month = 2,628,000,000 requests
  * Requests served from Cache (50% hit): 2,628,000,000 requests * 0.50 = 1,314,000,000 requests
  * Requests to Origin (50% miss): 2,628,000,000 requests * 0.50 = 1,314,000,000 requests
  * HTTPS Request Pricing (US East):
    * First 10,000,000 HTTPS requests: Free
    * Subsequent HTTPS requests: $0.0100 per 10,000 requests
  * Number of 10,000-request blocks: 1,304,000,000 / 10,000 = 130,400
  * Monthly Cost for Requests: 130,400 * $0.0100 = $1,304.00
* *Source: [AWS CloudFront Pricing](https://aws.amazon.com/cloudfront/pricing/)*

### Application Load Balancer
* Standard ALB: **$250.03/month**
  * $0.0225 per hour
  * $0.008 per LCU (load capacity units) per hour
  * LCU capacity is 25 new connections per second per LCU
  * Hourly cost: $0.0225/hour * 730 hours/month = $16.43 per month
  * LCU cost: 1000 (connections per second) / 25 (connects per LCU) * 0.008 per LCU per hour * 730 hours/month = $233.6
* *Source: [AWS Load Balancer Pricing](https://aws.amazon.com/elasticloadbalancing/pricing/)*

### AWS Lambda (Data Updater)
* Daily execution within free tier limits = **$0/month**
  * 1 million free requests
  * 400,000 GB-seconds of compute time
* *Source: [AWS Lambda Pricing](https://aws.amazon.com/lambda/pricing/)*

**Total Monthly Cost: ~$3052.01**
*Note, a big chunk of this cost ($1,304) is coming from Cloudfront. We might not need Cloudfront if the performance is good enough without it. Also, the actual average RPS might be lower than 1000/s*

---

## Data Update Process
The Python data updater script handles zero-downtime updates:
1. Downloads fresh IP data from provider URL
2. Creates staging table (`ip_locations_new`)
3. Imports and validates data in staging
4. Atomic table swap using `ALTER TABLE RENAME`
5. Drops old table
This approach ensures the API never sees incomplete data during updates.

---

## Performance Targets
* **Latency:** <50ms for cached requests, <200ms for database queries
* **Throughput:** 1000+ RPS with room for growth
* **Availability:** 99.9% uptime with Multi-AZ deployments
* **Cache hit rate:** Expected 70-80% for common IPs

---

## Future Steps

### Monitoring & Observability
* Prometheus deployment on ECS for metrics collection
* Grafana dashboards for visualization
* Custom metrics: API response times, cache hit rates, database performance
* Alert rules for high latency, error rates, and capacity thresholds

### Scaling Considerations
* The C++ application already implements database connection pooling
* ECS Fargate uses auto scaling to automatically adjust the number of running tasks for the API service
* Cache warming strategies for new data
* Geographic distribution for global users

### Security
* AWS WAF integration with CloudFront and ALB for DDoS protection and rate limiting