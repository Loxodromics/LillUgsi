#include "vector3.h"
#include <cmath> /// For sqrt

namespace lillugsi::planet {

/// Default constructor initializes to zero vector
Vector3::Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

/// Constructor with parameters
Vector3::Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

/// Addition
Vector3 Vector3::operator+(const Vector3& other) const {
	return {x + other.x, y + other.y, z + other.z};
}

/// Subtraction
Vector3 Vector3::operator-(const Vector3& other) const {
	return {x - other.x, y - other.y, z - other.z};
}

/// Scalar multiplication
Vector3 Vector3::operator*(float scalar) const {
	return {x * scalar, y * scalar, z * scalar};
}

/// Dot product
float Vector3::dot(const Vector3& other) const {
	return x * other.x + y * other.y + z * other.z;
}

/// Cross product
Vector3 Vector3::cross(const Vector3& other) const {
	return {y * other.z - z * other.y,
				z * other.x - x * other.z,
				x * other.y - y * other.x};
}

/// Normalize the vector
void Vector3::normalize() {
	float length = sqrt(x * x + y * y + z * z);
	if (length > 0) {
		x /= length;
		y /= length;
		z /= length;
	}
}

Vector3 Vector3::normalized() const {
	float length = sqrt(x * x + y * y + z * z);
	if (length > 0) {
		return {x / length, y / length, z / length};
	} else {
		return {x, y, z}; /// Return the original vector if length is 0 to avoid division by zero
	}
}
} /// namespace lillugsi::planet