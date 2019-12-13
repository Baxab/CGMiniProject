
#pragma once

#include "Util.h"

using namespace DirectX;


class Camera
{
public:

	Camera();
	~Camera();

	XMFLOAT3 GetPosition()const;
	void SetPosition(float x, float y, float z);

	XMFLOAT3 GetRight()const;
	XMFLOAT3 GetUp()const;
	XMFLOAT3 GetLook()const;

	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWidth()const;
	float GetNearHeight()const;
	float GetFarWidth()const;
	float GetFarHeight()const;

	void SetFrustum(float fovY, float aspect, float zn, float zf);

	// Get View/Proj matrices.
	XMMATRIX GetView()const;
	XMMATRIX GetProj()const;

	void LeftAndRight(float d);
	void ForwardAndBackward(float d);
	void Pitch(float alpha);
	void Yaw(float alpha);

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();

private:

	// Camera coordinate system with coordinates relative to world space.
	XMFLOAT3 m_pos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 m_right = { 1.0f, 0.0f, 0.0f };
	XMFLOAT3 m_up = { 0.0f, 1.0f, 0.0f };
	XMFLOAT3 m_look = { 0.0f, 0.0f, 1.0f };

	XMFLOAT4X4 viewMatrix = UtilMath::Identity4x4();
	XMFLOAT4X4 projMatrix = UtilMath::Identity4x4();

	// frustum properties.
	float m_near;
	float m_far;
	float m_nearHeight;
	float m_farHeight;
	float m_ratio;
	float m_fovY;

	bool isDirty = true;

};

