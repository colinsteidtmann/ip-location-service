FROM python:3.11-slim-bookworm

WORKDIR /app

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y --no-install-recommends libpq-dev curl gcc \
    && apt-get clean && rm -rf /var/lib/apt/lists/*


COPY requirements.txt .

RUN pip install --no-cache-dir -r requirements.txt


COPY scripts/ /app/scripts/

CMD ["python", "/app/scripts/update_data.py", "--run-now", "&&", "python", "/app/scripts/update_data.py", "--schedule"]
