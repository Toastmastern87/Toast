#include "tpch.h"

#include "Vector.h"

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//		  VECTOR3	  	  //////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	Vector3::Vector3(float xIn, float yIn, float zIn, float wIn) {
		x = (double)xIn;
		y = (double)yIn;
		z = (double)zIn;
		w = (double)wIn;
	}

	Vector3::Vector3(DirectX::XMVECTOR vec) {
		x = DirectX::XMVectorGetX(vec);
		y = DirectX::XMVectorGetY(vec);
		z = DirectX::XMVectorGetZ(vec);
		w = 1.0;
	}

	Vector3::Vector3(DirectX::XMFLOAT3 vec) {
		x = vec.x;
		y = vec.y;
		z = vec.z;
		w = 1.0;
	}

	Vector3::Vector3(std::initializer_list<double> list)
	{
		auto it = list.begin();

		x = (it != list.end()) ? static_cast<double>(*it++) : 0.0;
		y = (it != list.end()) ? static_cast<double>(*it++) : 0.0;
		z = (it != list.end()) ? static_cast<double>(*it++) : 0.0;
		w = 1.0;
	}

	double Vector3::Magnitude() const {
		return sqrt(x * x + y * y + z * z);
	}

	double Vector3::MagnitudeSqrt() const {
		return x * x + y * y + z * z;
	}

	Vector3 Vector3::Normalize(const Vector3& vec) {
		Vector3 result = vec;
		double magnitude = vec.Magnitude();
		if (magnitude > 0.0)
		{
			result.x /= magnitude;
			result.y /= magnitude;
			result.z /= magnitude;
		}

		return result;
	}

	Vector3 Vector3::Normalize(std::initializer_list<double> list)
	{
		Vector3 vec(list);
		return Normalize(vec);
	}

	Vector3 Vector3::HomogeneousNormalize() const {
		if (w == 0.0) return *this;  // Avoid division by zero
		return Vector3(x / w, y / w, z / w, 1.0);
	}

	Vector3 Vector3::Cross(const Vector3& a, const Vector3& b) {
		return Vector3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		);
	}

	double Vector3::Dot(const Vector3& a, const Vector3& b) {
		return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
	}

	Vector3 Vector3::Rotate(const Vector3 vec, const Quaternion& q) {
		Quaternion vecQuat(vec.x, vec.y, vec.z, 0.0);
		Quaternion qInv = q.Conjugate();
		Quaternion resultQuat = q * vecQuat * qInv;

		return Vector3(resultQuat.x, resultQuat.y, resultQuat.z);
	}

	void Vector3::ToString() 
	{
		TOAST_CORE_INFO("Vector3: %lf, %lf, %lf", x, y, z);
	}

	void Vector3::ToString(const std::string& label)
	{
		TOAST_CORE_INFO("%s: %lf, %lf, %lf", label.c_str(), x, y, z);
	}

	void Vector3::ToString(const std::string& label) const
	{
		TOAST_CORE_INFO("%s: %lf, %lf, %lf", label.c_str(), x, y, z);
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//		  VECTOR2	  	  //////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	Vector2::Vector2(float xIn, float yIn) {
		x = (double)xIn;
		y = (double)yIn;
	}

	Vector2::Vector2(DirectX::XMVECTOR vec) {
		x = DirectX::XMVectorGetX(vec);
		y = DirectX::XMVectorGetY(vec);
	}

	Vector2::Vector2(DirectX::XMFLOAT2 vec) {
		x = vec.x;
		y = vec.y;
	}

	Vector2::Vector2(std::initializer_list<double> list)
	{
		auto it = list.begin();

		x = (it != list.end()) ? static_cast<double>(*it++) : 0.0;
		y = (it != list.end()) ? static_cast<double>(*it++) : 0.0;
	}

	Vector2::Vector2(std::initializer_list<uint32_t> list)
	{
		auto it = list.begin();

		x = (it != list.end()) ? static_cast<double>(*it++) : 0;
		y = (it != list.end()) ? static_cast<double>(*it++) : 0;
	}

	double Vector2::Magnitude() const {
		return sqrt(x * x + y * y);
	}

	Vector2 Vector2::Normalize(Vector2& vec) {
		double magnitude = vec.Magnitude();
		if (magnitude > 0.0)
		{
			vec.x /= magnitude;
			vec.y /= magnitude;
		}

		return vec;
	}

	Vector2 Vector2::Normalize(std::initializer_list<double> list)
	{
		Vector2 vec(list);
		return Normalize(vec);
	}

	Vector2 Vector2::Cross(const Vector2& a, const Vector2& b) {
		return Vector2(
			a.x * b.y - a.y * b.x
		);
	}

	double Vector2::Dot(const Vector2& a, const Vector2& b) {
		return a.x * b.x + a.y * b.y;
	}

	void Vector2::ToString()
	{
		TOAST_CORE_INFO("Vector2: %lf, %lf", x, y);
	}

	void Vector2::ToString(const std::string& label)
	{
		TOAST_CORE_INFO("%s: %lf, %lf", label.c_str(), x, y);
	}
}