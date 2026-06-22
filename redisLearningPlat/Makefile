.PHONY: dev install lint format check test build clean

# Development
dev-backend:
	cd backend && uvicorn app.main:app --reload --host 0.0.0.0 --port 8000

dev-frontend:
	cd frontend && pnpm dev

dev: dev-backend dev-frontend

# Install dependencies
install:
	cd frontend && pnpm install
	cd backend && pip install -r requirements.txt

# Linting
lint-frontend:
	cd frontend && pnpm lint

lint-backend:
	cd backend && ruff check app/ tests/

lint: lint-frontend lint-backend

# Formatting
format-frontend:
	cd frontend && pnpm format

format-backend:
	cd backend && ruff format app/ tests/

format: format-frontend format-backend

# Type checking
check-frontend:
	cd frontend && pnpm typecheck

check: check-frontend lint

# Testing
test-frontend:
	cd frontend && pnpm test

test-backend:
	cd backend && pytest tests/ -v

test: test-frontend test-backend

# Build
build:
	cd frontend && pnpm build

# Docker
docker-up:
	docker-compose up -d

docker-down:
	docker-compose down

# Clean
clean:
	rm -rf frontend/dist frontend/node_modules backend/__pycache__ backend/.venv
	find backend -type d -name __pycache__ -exec rm -rf {} +
