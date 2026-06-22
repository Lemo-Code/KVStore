from pydantic_settings import BaseSettings
from typing import List


class Settings(BaseSettings):
    # Database
    database_url: str = "sqlite+aiosqlite:///./learn_redis.db"

    # Auth
    secret_key: str = "dev-secret-key-change-in-production"
    access_token_expire_minutes: int = 30
    refresh_token_expire_days: int = 7

    # AI
    openai_api_key: str = ""
    openai_base_url: str = "https://api.openai.com/v1"
    default_ai_model: str = "gpt-4o-mini"

    # CORS
    cors_origins: List[str] = ["http://localhost:5173", "http://localhost:3000"]

    # Admin test account
    admin_username: str = "admin"
    admin_password: str = "admin123"
    admin_email: str = "admin@learn-redis.local"

    model_config = {"env_file": ".env", "env_file_encoding": "utf-8"}


settings = Settings()
