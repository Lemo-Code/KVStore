import uuid
from datetime import datetime, timezone
from sqlalchemy import String, DateTime, ForeignKey, Text, Integer
from sqlalchemy.orm import Mapped, mapped_column, relationship
from ..db.session import Base


class KnowledgeCategory(Base):
    __tablename__ = "knowledge_categories"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    name: Mapped[str] = mapped_column(String(100), nullable=False)
    slug: Mapped[str] = mapped_column(String(100), unique=True, nullable=False)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)
    parent_id: Mapped[str | None] = mapped_column(String(36), ForeignKey("knowledge_categories.id"), nullable=True)
    sort_order: Mapped[int] = mapped_column(Integer, default=0)
    icon: Mapped[str | None] = mapped_column(String(50), nullable=True)

    articles: Mapped[list["KnowledgeArticle"]] = relationship(back_populates="category", lazy="selectin")

    def to_tree_node(self, article_count: int = 0):
        return {
            "id": self.id,
            "name": self.name,
            "type": "category",
            "slug": self.slug,
            "articleCount": article_count,
            "icon": self.icon,
            "children": [],
        }


class KnowledgeArticle(Base):
    __tablename__ = "knowledge_articles"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    title: Mapped[str] = mapped_column(String(200), nullable=False)
    slug: Mapped[str] = mapped_column(String(200), unique=True, nullable=False)
    content_md: Mapped[str] = mapped_column(Text, nullable=False)
    excerpt: Mapped[str | None] = mapped_column(String(500), nullable=True)
    category_id: Mapped[str] = mapped_column(String(36), ForeignKey("knowledge_categories.id"), nullable=False, index=True)
    difficulty: Mapped[str] = mapped_column(String(20), default="beginner")
    tags: Mapped[str | None] = mapped_column(String(500), nullable=True)
    author_id: Mapped[str] = mapped_column(String(36), ForeignKey("users.id"), nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=lambda: datetime.now(timezone.utc)
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=lambda: datetime.now(timezone.utc)
    )
    view_count: Mapped[int] = mapped_column(Integer, default=0)

    category: Mapped["KnowledgeCategory"] = relationship(back_populates="articles")

    def to_dict(self):
        return {
            "id": self.id,
            "title": self.title,
            "slug": self.slug,
            "content": self.content_md,
            "excerpt": self.excerpt or "",
            "categoryId": self.category_id,
            "categoryName": self.category.name if self.category else "",
            "difficulty": self.difficulty,
            "tags": self.tags.split(",") if self.tags else [],
            "authorId": self.author_id,
            "authorName": "",
            "createdAt": self.created_at.isoformat(),
            "updatedAt": self.updated_at.isoformat(),
            "viewCount": self.view_count,
            "prevArticle": None,
            "nextArticle": None,
            "toc": [],
        }
