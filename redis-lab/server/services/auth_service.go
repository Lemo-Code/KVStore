package services

import (
	"errors"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"

	"redis-lab/server/config"
	"redis-lab/server/database"
	"redis-lab/server/models"
)

type AuthService struct {
	cfg *config.Config
}

func NewAuthService(cfg *config.Config) *AuthService {
	return &AuthService{cfg: cfg}
}

func (s *AuthService) Register(req models.RegisterRequest) (*models.User, error) {
	// Check if username already exists
	var existing models.User
	if err := database.DB.Where("username = ?", req.Username).First(&existing).Error; err == nil {
		return nil, errors.New("用户名已存在")
	}
	if err := database.DB.Where("email = ?", req.Email).First(&existing).Error; err == nil {
		return nil, errors.New("邮箱已被注册")
	}

	// Hash password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		return nil, errors.New("密码加密失败")
	}

	user := models.User{
		Username: req.Username,
		Email:    req.Email,
		Password: string(hashedPassword),
	}

	if err := database.DB.Create(&user).Error; err != nil {
		return nil, errors.New("创建用户失败")
	}

	return &user, nil
}

func (s *AuthService) Login(req models.LoginRequest) (*models.LoginResponse, error) {
	var user models.User
	if err := database.DB.Where("username = ?", req.Username).First(&user).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, errors.New("用户名或密码错误")
		}
		return nil, errors.New("登录失败")
	}

	// Verify password
	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(req.Password)); err != nil {
		return nil, errors.New("用户名或密码错误")
	}

	// Generate JWT
	token, err := s.generateToken(user.ID)
	if err != nil {
		return nil, errors.New("生成令牌失败")
	}

	return &models.LoginResponse{
		Token: token,
		User:  user,
	}, nil
}

func (s *AuthService) GetUserByID(userID uint) (*models.User, error) {
	var user models.User
	if err := database.DB.First(&user, userID).Error; err != nil {
		return nil, err
	}
	return &user, nil
}

func (s *AuthService) generateToken(userID uint) (string, error) {
	claims := jwt.MapClaims{
		"user_id": userID,
		"exp":     time.Now().Add(time.Duration(s.cfg.JWT.ExpireHour) * time.Hour).Unix(),
		"iat":     time.Now().Unix(),
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(s.cfg.JWT.Secret))
}

func (s *AuthService) ValidateToken(tokenStr string) (uint, error) {
	token, err := jwt.Parse(tokenStr, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, errors.New("无效的签名方法")
		}
		return []byte(s.cfg.JWT.Secret), nil
	})
	if err != nil {
		return 0, err
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok || !token.Valid {
		return 0, errors.New("无效的令牌")
	}

	userIDFloat, ok := claims["user_id"].(float64)
	if !ok {
		return 0, errors.New("无效的令牌载荷")
	}

	return uint(userIDFloat), nil
}
