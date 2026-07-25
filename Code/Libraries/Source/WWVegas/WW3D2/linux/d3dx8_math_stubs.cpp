/*
 * In-process D3DX8 matrix/vector helpers (no d3dx8.dll).
 */
#include <d3dx8.h>
#include <cmath>
#include <cstring>

extern "C" {

static void d3dx_mat_copy(D3DXMATRIX *out, const D3DXMATRIX *in)
{
	memcpy(out, in, sizeof(D3DXMATRIX));
}

D3DXMATRIX *WINAPI D3DXMatrixMultiply(D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2)
{
	D3DXMATRIX result;
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k) {
				sum += (*pM1)(row, k) * (*pM2)(k, col);
			}
			result(row, col) = sum;
		}
	}
	*pOut = result;
	return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixTranspose(D3DXMATRIX *pOut, const D3DXMATRIX *pM)
{
	D3DXMATRIX result;
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result(row, col) = (*pM)(col, row);
		}
	}
	*pOut = result;
	return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixScaling(D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz)
{
	D3DXMatrixIdentity(pOut);
	(*pOut)(0, 0) = sx;
	(*pOut)(1, 1) = sy;
	(*pOut)(2, 2) = sz;
	return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixTranslation(D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z)
{
	D3DXMatrixIdentity(pOut);
	(*pOut)(3, 0) = x;
	(*pOut)(3, 1) = y;
	(*pOut)(3, 2) = z;
	return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixRotationZ(D3DXMATRIX *pOut, FLOAT angle)
{
	const float c = cosf(angle);
	const float s = sinf(angle);
	D3DXMatrixIdentity(pOut);
	(*pOut)(0, 0) = c;
	(*pOut)(0, 1) = s;
	(*pOut)(1, 0) = -s;
	(*pOut)(1, 1) = c;
	return pOut;
}

D3DXMATRIX *WINAPI D3DXMatrixInverse(D3DXMATRIX *pOut, FLOAT *pDeterminant, const D3DXMATRIX *pM)
{
	float m[4][4];
	float inv[4][4];
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			m[r][c] = (*pM)(r, c);
		}
	}

	const float det =
		m[0][0] * (m[1][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
			m[1][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) +
			m[1][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1])) -
		m[0][1] * (m[1][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
			m[1][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
			m[1][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0])) +
		m[0][2] * (m[1][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) -
			m[1][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
			m[1][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0])) -
		m[0][3] * (m[1][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) -
			m[1][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) +
			m[1][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));

	if (pDeterminant) {
		*pDeterminant = det;
	}
	if (fabsf(det) < 1e-12f) {
		return NULL;
	}

	const float invDet = 1.0f / det;
	inv[0][0] = invDet * (m[1][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
		m[1][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) +
		m[1][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]));
	inv[0][1] = -invDet * (m[0][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
		m[0][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) +
		m[0][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]));
	inv[0][2] = invDet * (m[0][1] * (m[1][2] * m[3][3] - m[1][3] * m[3][2]) -
		m[0][2] * (m[1][1] * m[3][3] - m[1][3] * m[3][1]) +
		m[0][3] * (m[1][1] * m[3][2] - m[1][2] * m[3][1]));
	inv[0][3] = -invDet * (m[0][1] * (m[1][2] * m[2][3] - m[1][3] * m[2][2]) -
		m[0][2] * (m[1][1] * m[2][3] - m[1][3] * m[2][1]) +
		m[0][3] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]));
	inv[1][0] = -invDet * (m[1][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
		m[1][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
		m[1][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]));
	inv[1][1] = invDet * (m[0][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
		m[0][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
		m[0][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]));
	inv[1][2] = -invDet * (m[0][0] * (m[1][2] * m[3][3] - m[1][3] * m[3][2]) -
		m[0][2] * (m[1][0] * m[3][3] - m[1][3] * m[3][0]) +
		m[0][3] * (m[1][0] * m[3][2] - m[1][2] * m[3][0]));
	inv[1][3] = invDet * (m[0][0] * (m[1][2] * m[2][3] - m[1][3] * m[2][2]) -
		m[0][2] * (m[1][0] * m[2][3] - m[1][3] * m[2][0]) +
		m[0][3] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]));
	inv[2][0] = invDet * (m[1][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) -
		m[1][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
		m[1][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
	inv[2][1] = -invDet * (m[0][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) -
		m[0][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
		m[0][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
	inv[2][2] = invDet * (m[0][0] * (m[1][1] * m[3][3] - m[1][3] * m[3][1]) -
		m[0][1] * (m[1][0] * m[3][3] - m[1][3] * m[3][0]) +
		m[0][3] * (m[1][0] * m[3][1] - m[1][1] * m[3][0]));
	inv[2][3] = -invDet * (m[0][0] * (m[1][1] * m[2][3] - m[1][3] * m[2][1]) -
		m[0][1] * (m[1][0] * m[2][3] - m[1][3] * m[2][0]) +
		m[0][3] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]));
	inv[3][0] = -invDet * (m[1][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) -
		m[1][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) +
		m[1][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
	inv[3][1] = invDet * (m[0][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) -
		m[0][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) +
		m[0][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
	inv[3][2] = -invDet * (m[0][0] * (m[1][1] * m[3][2] - m[1][2] * m[3][1]) -
		m[0][1] * (m[1][0] * m[3][2] - m[1][2] * m[3][0]) +
		m[0][2] * (m[1][0] * m[3][1] - m[1][1] * m[3][0]));
	inv[3][3] = invDet * (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
		m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
		m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]));

	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			(*pOut)(r, c) = inv[r][c];
		}
	}
	return pOut;
}

D3DXVECTOR4 *WINAPI D3DXVec4Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV, const D3DXMATRIX *pM)
{
	const float x = pV->x;
	const float y = pV->y;
	const float z = pV->z;
	const float w = pV->w;
	pOut->x = x * (*pM)(0, 0) + y * (*pM)(1, 0) + z * (*pM)(2, 0) + w * (*pM)(3, 0);
	pOut->y = x * (*pM)(0, 1) + y * (*pM)(1, 1) + z * (*pM)(2, 1) + w * (*pM)(3, 1);
	pOut->z = x * (*pM)(0, 2) + y * (*pM)(1, 2) + z * (*pM)(2, 2) + w * (*pM)(3, 2);
	pOut->w = x * (*pM)(0, 3) + y * (*pM)(1, 3) + z * (*pM)(2, 3) + w * (*pM)(3, 3);
	return pOut;
}

HRESULT WINAPI D3DXAssembleShader(LPCVOID pSrcData, UINT SrcDataLen, DWORD Flags, LPD3DXBUFFER *ppConstants,
	LPD3DXBUFFER *ppCompiledShader, LPD3DXBUFFER *ppCompilationErrors)
{
	(void)pSrcData;
	(void)SrcDataLen;
	(void)Flags;
	if (ppConstants) {
		*ppConstants = NULL;
	}
	if (ppCompiledShader) {
		*ppCompiledShader = NULL;
	}
	if (ppCompilationErrors) {
		*ppCompilationErrors = NULL;
	}
	return E_FAIL;
}

}
