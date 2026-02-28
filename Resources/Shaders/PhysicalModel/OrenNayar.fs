/*
 Copyright (c) 2013 yvt

 This file is part of OpenSpades.

 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with OpenSpades.	 If not, see <http://www.gnu.org/licenses/>.

 */

// http://en.wikipedia.org/wiki/Oren–Nayar_reflectance_model
float OrenNayar(float sigma, float dotLight, float dotEye) {
	float sigma2 = sigma * sigma;
	float A = 1.0 - 0.5 * sigma2 / (sigma2 + 0.33);
	float B = 0.45 * sigma2 / (sigma2 + 0.09);
	float scale = 1.0 / A;
	float scaledB = B * scale;

	vec2 dotLightEye = clamp(vec2(dotLight, dotEye), 0.0, 1.0);
	vec2 sinLightEye = sqrt(1.0 - dotLightEye * dotLightEye);
	
	float alphaSin = max(sinLightEye.x, sinLightEye.y);
	float betaCos = max(dotLightEye.x, dotLightEye.y);
	float betaSin = min(sinLightEye.x, sinLightEye.y);
	float betaTan = betaSin / max(betaCos, 0.00001);

	// cos(dotLight - dotEye)
	vec4 vecs = vec4(dotLightEye, sinLightEye);
	float diffCos = max(dot(vecs.xz, vecs.yw), 0.0);

	// compute
	return dotLight * (1.0 + scaledB * diffCos * alphaSin * betaTan);
}