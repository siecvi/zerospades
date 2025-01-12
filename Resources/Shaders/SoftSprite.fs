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
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.

 */

uniform sampler2D depthTexture;
uniform sampler2D mainTexture;
uniform vec3 fogColor;
uniform vec2 zNearFar;

varying vec4 color;
varying vec4 texCoord;
varying vec4 fogDensity;
varying vec4 depthRange;

float decodeDepth(float w, float near, float far) {
	return 1.0 / mix(far, near, w);
}

float depthAt(vec2 pt) {
	float w = texture2D(depthTexture, pt).x;
	return decodeDepth(w, zNearFar.x, zNearFar.y);
}

void main() {
	// get depth
	float depth = depthAt(texCoord.zw);
	if (depth < depthRange.y)
		discard;

	gl_FragColor = texture2D(mainTexture, texCoord.xy);

	// linearize
#if LINEAR_FRAMEBUFFER
	gl_FragColor.xyz *= gl_FragColor.xyz;
#endif

	gl_FragColor.xyz *= gl_FragColor.w; // premultiplied alpha
	gl_FragColor *= color;

	vec4 fogColorP = vec4(fogColor, 1.0);

	// linearize
#if LINEAR_FRAMEBUFFER
	fogColorP.xyz *= fogColorP.xyz;
#endif

	fogColorP *= gl_FragColor.w; // premultiplied alpha

	gl_FragColor = mix(gl_FragColor, fogColorP, fogDensity);

	float soft = depth * depthRange.z + depthRange.x;
	soft = smoothstep(0.0, 1.0, soft);
	gl_FragColor *= soft;

	if (dot(gl_FragColor, vec4(1.0)) < 0.002)
		discard;
}