#pragma once

namespace lillugsi::planet {
class Vector3 {
public:
	float x, y, z;

	/// Constructors
	Vector3();
	Vector3(float x, float y, float z);

	/// Destructor
	~Vector3() = default;

	/// Copy constructor
	Vector3(const Vector3& other) = default;

	/// Assignment operator
	Vector3& operator=(const Vector3& other) = default;

	/// Vector operations
	Vector3 operator+(const Vector3& other) const;
	Vector3 operator-(const Vector3& other) const;
	Vector3 operator*(float scalar) const;

	/// Dot product
	[[nodiscard]] float dot(const Vector3& other) const;

	/// Cross product
	[[nodiscard]] Vector3 cross(const Vector3& other) const;

	/// Normalize the vector
	void normalize();
	[[nodiscard]] Vector3 normalized() const;
};
} /// namespace lillugsi::planet

