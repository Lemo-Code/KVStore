import uuid
from datetime import datetime, timezone
from sqlalchemy import String, Boolean, DateTime, ForeignKey, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship
from ..db.session import Base


class ChatRoom(Base):
    __tablename__ = "chat_rooms"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    name: Mapped[str] = mapped_column(String(100), nullable=False)
    is_group: Mapped[bool] = mapped_column(Boolean, default=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=lambda: datetime.now(timezone.utc)
    )

    members: Mapped[list["RoomMember"]] = relationship(back_populates="room", lazy="selectin")
    messages: Mapped[list["ChatMessage"]] = relationship(back_populates="room", lazy="selectin")

    def to_dict(self):
        return {
            "id": self.id,
            "name": self.name,
            "isGroup": self.is_group,
            "createdAt": self.created_at.isoformat(),
            "members": [m.to_dict() for m in self.members],
            "unreadCount": 0,
        }


class RoomMember(Base):
    __tablename__ = "room_members"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    room_id: Mapped[str] = mapped_column(String(36), ForeignKey("chat_rooms.id"), nullable=False)
    user_id: Mapped[str] = mapped_column(String(36), ForeignKey("users.id"), nullable=False)
    joined_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=lambda: datetime.now(timezone.utc)
    )

    room: Mapped["ChatRoom"] = relationship(back_populates="members")

    def to_dict(self):
        return {
            "id": self.user_id,
            "username": "",
            "email": "",
        }


class ChatMessage(Base):
    __tablename__ = "chat_messages"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    room_id: Mapped[str] = mapped_column(String(36), ForeignKey("chat_rooms.id"), nullable=False, index=True)
    user_id: Mapped[str] = mapped_column(String(36), ForeignKey("users.id"), nullable=False)
    content: Mapped[str] = mapped_column(Text, nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=lambda: datetime.now(timezone.utc)
    )

    room: Mapped["ChatRoom"] = relationship(back_populates="messages")

    def to_dict(self, username: str = "", avatar_url: str | None = None):
        return {
            "id": self.id,
            "roomId": self.room_id,
            "userId": self.user_id,
            "userName": username,
            "userAvatar": avatar_url,
            "content": self.content,
            "createdAt": self.created_at.isoformat(),
            "status": "delivered",
        }
