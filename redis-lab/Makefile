.PHONY: dev-backend dev-frontend dev-redis dev-all build-backend build-frontend docker-up docker-down clean

# ============================================================
# Development (local, without Docker)
# ============================================================

# Start both Redis instances locally (requires redis-server installed)
dev-redis:
	@echo "Starting storage Redis on :6379..."
	redis-server --port 6379 --save "" --loglevel warning --daemonize yes
	@echo "Starting learn Redis on :6380..."
	redis-server --port 6380 --save "" --loglevel warning --daemonize yes
	@echo "✅ Both Redis instances started"

# Stop local Redis instances
dev-redis-stop:
	redis-cli -p 6379 shutdown 2>/dev/null || true
	redis-cli -p 6380 shutdown 2>/dev/null || true
	@echo "Redis instances stopped"

# Start Go backend
dev-backend:
	cd server && go run .

# Start React frontend
dev-frontend:
	cd web && npm install && npm run dev

# Start everything locally (3 terminals needed, or use docker-up)
dev-all:
	@echo "Run in 3 terminals:"
	@echo "  1. make dev-redis"
	@echo "  2. make dev-backend"
	@echo "  3. make dev-frontend"

# ============================================================
# Build
# ============================================================

build-backend:
	cd server && go build -o redis-lab-server .

build-frontend:
	cd web && npm install && npm run build

# ============================================================
# Docker
# ============================================================

docker-up:
	export AI_API_KEY=${AI_API_KEY:-} && \
	docker compose up -d

docker-down:
	docker compose down

docker-logs:
	docker compose logs -f

# ============================================================
# Clean
# ============================================================

clean:
	rm -f server/redis-lab-server
	rm -rf web/dist
	rm -rf server/data
