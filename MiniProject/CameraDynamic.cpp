
#include "CameraDynamic.h"

using namespace DirectX;

Camera::Camera()
{
	SetFrustum(0.25f*UtilMath::Pi, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{
}

XMFLOAT3 Camera::GetPosition()const
{
	return m_pos;
}

void Camera::SetPosition(float x, float y, float z)
{
	m_pos = XMFLOAT3(x, y, z);
	isDirty = true;
}

XMFLOAT3 Camera::GetRight()const
{
	return m_right;
}

XMFLOAT3 Camera::GetUp()const
{
	return m_up;
}

XMFLOAT3 Camera::GetLook()const
{
	return m_look;
}

float Camera::GetNearZ()const
{
	return m_near;
}

float Camera::GetFarZ()const
{
	return m_far;
}

float Camera::GetAspect()const
{
	return m_ratio;
}

float Camera::GetFovY()const
{
	return m_fovY;
}

float Camera::GetFovX()const
{
	float halfWidth = 0.5f*GetNearWidth();
	return 2.0f*atan(halfWidth / m_near);
}

float Camera::GetNearWidth()const
{
	return m_ratio * m_nearHeight;
}

float Camera::GetNearHeight()const
{
	return m_nearHeight;
}

float Camera::GetFarWidth()const
{
	return m_ratio * m_farHeight;
}

float Camera::GetFarHeight()const
{
	return m_farHeight;
}

void Camera::SetFrustum(float fovY, float aspect, float zn, float zf)
{
	m_fovY = fovY;
	m_ratio = aspect;
	m_near = zn;
	m_far = zf;
	//m_nearHeight = 2.0f * m_near * tanf(0.5f*m_fovY);
	//m_farHeight = 2.0f * m_far * tanf(0.5f*m_fovY);

	XMStoreFloat4x4(&projMatrix, XMMatrixPerspectiveFovLH(m_fovY, m_ratio, m_near, m_far));
}

XMMATRIX Camera::GetView()const
{
	return XMLoadFloat4x4(&viewMatrix);
}

XMMATRIX Camera::GetProj()const
{
	return XMLoadFloat4x4(&projMatrix);
}


void Camera::LeftAndRight(float d)
{
	XMStoreFloat3(&m_pos, XMVectorMultiplyAdd(XMVectorReplicate(d), XMLoadFloat3(&m_right), XMLoadFloat3(&m_pos)));
	isDirty = true;
}

void Camera::ForwardAndBackward(float d)
{
	XMStoreFloat3(&m_pos, XMVectorMultiplyAdd(XMVectorReplicate(d), XMLoadFloat3(&m_look), XMLoadFloat3(&m_pos)));
	isDirty = true;
}

void Camera::Pitch(float alpha)
{
	// Rotate up and look vector about the right vector.
	XMMATRIX rotAxis = XMMatrixRotationAxis(XMLoadFloat3(&m_right), alpha);

	XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rotAxis));
	XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), rotAxis));

	isDirty = true;
}

void Camera::Yaw(float alpha)
{
	// Rotate the basis vectors about the world y-axis.
	XMMATRIX rotAxis = XMMatrixRotationY(alpha);

	XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), rotAxis));
	XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rotAxis));
	XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), rotAxis));

	isDirty = true;
}

void Camera::UpdateViewMatrix()
{
	if (isDirty)
	{
		XMVECTOR right = XMLoadFloat3(&m_right);
		XMVECTOR up = XMLoadFloat3(&m_up);
		XMVECTOR look = XMLoadFloat3(&m_look);
		XMVECTOR position = XMLoadFloat3(&m_pos);

		// Normalize and calculate orthonormal basis
		look = XMVector3Normalize(look);
		up = XMVector3Normalize(XMVector3Cross(look, right));
		right = XMVector3Cross(up, look);

		std::wstring text11 =
			L"POSITION = " + std::to_wstring(XMVectorGetY(up)) + L" \n";

		::OutputDebugString(text11.c_str());

		// viewMatrix entries.
		std::wstring text =
			L"POSITION = " + std::to_wstring(XMVectorGetY(position)) + L" \n";

		::OutputDebugString(text.c_str());


		float x = -XMVectorGetX(XMVector3Dot(position, right));
		float y = -XMVectorGetX(XMVector3Dot(position, up));
		float z = -XMVectorGetX(XMVector3Dot(position, look));

		std::wstring text2 =
			L"POSITION 2 = " + std::to_wstring(y) + L" \n";

		::OutputDebugString(text2.c_str());

		viewMatrix(0, 0) = XMVectorGetX(right);
		viewMatrix(1, 0) = XMVectorGetY(right);
		viewMatrix(2, 0) = XMVectorGetZ(right);
		viewMatrix(3, 0) = x;
		viewMatrix(0, 1) = XMVectorGetX(up);
		viewMatrix(1, 1) = XMVectorGetY(up);
		viewMatrix(2, 1) = XMVectorGetZ(up);
		viewMatrix(3, 1) = y;
		viewMatrix(0, 2) = XMVectorGetX(look);
		viewMatrix(1, 2) = XMVectorGetY(look);
		viewMatrix(2, 2) = XMVectorGetZ(look);
		viewMatrix(3, 2) = z;
		viewMatrix(0, 3) = 0.0f;
		viewMatrix(1, 3) = 0.0f;
		viewMatrix(2, 3) = 0.0f;
		viewMatrix(3, 3) = 1.0f;

		XMStoreFloat3(&m_right, right);
		XMStoreFloat3(&m_up, up);
		XMStoreFloat3(&m_look, look);

		isDirty = false;
	}
}


