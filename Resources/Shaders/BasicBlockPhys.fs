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

varying vec4 color;
varying vec2 ambientOcclusionCoord;
varying vec3 fogDensity;

varying vec3 viewSpaceCoord;
varying vec3 viewSpaceNormal;
uniform vec3 viewSpaceLight;

varying vec3 reflectionDir;

uniform sampler2D ambientOcclusionTexture;
uniform vec3 fogColor;

vec3 EvaluateSunLight();
vec3 EvaluateAmbientLight(float detailAmbientOcclusion);
vec3 EvaluateDirectionalAmbientLight(float detailAmbientOcclusion, vec3 direction);

float OrenNayar(float sigma, float dotLight, float dotEye);
float CockTorrance(vec3 eyeVec, vec3 lightVec, vec3 normal);

void main() {
	// color is linear
	gl_FragColor = vec4(color.xyz, 1.0);

	vec3 eye = normalize(viewSpaceCoord);
	float dotNE = dot(viewSpaceNormal, eye);
	float dotNL = color.w;

	vec3 shading = vec3(OrenNayar(0.8, dotNL, -dotNE));
	vec3 sunLight = EvaluateSunLight();
	shading *= sunLight;

	float ao = texture2D(ambientOcclusionTexture, ambientOcclusionCoord).x;
	shading += EvaluateAmbientLight(ao);

	// apply diffuse shading
	gl_FragColor.xyz *= shading;

	// Fresnel term (be careful; viewSpaceNormal is a surface normal, not a microfacet one)
	float fresnel2 = 1.0 + dotNE;
	float fresnel = 0.03 + 0.1 * fresnel2 * fresnel2;

	// Specular shading (blurred reflections, assuming roughness is high)
	vec3 reflectWS = normalize(reflectionDir);
	vec3 reflection = EvaluateDirectionalAmbientLight(ao, reflectWS);

	gl_FragColor.xyz = mix(gl_FragColor.xyz, reflection, fresnel);

	// Diffuse/specular shading for sunlight
	if (dot(sunLight, vec3(1.0)) > 0.001 && dotNL > 0.1)
		gl_FragColor.xyz += sunLight * CockTorrance(-eye, viewSpaceLight, viewSpaceNormal);

	// apply fog
	gl_FragColor.xyz = max(mix(gl_FragColor.xyz, fogColor, fogDensity), 0.0);

	// gamma correct
#if !LINEAR_FRAMEBUFFER
	gl_FragColor.xyz = sqrt(gl_FragColor.xyz);
#endif

#if USE_HDR
	// somehow denormal occurs, so detect it here and remove
	// (denormal destroys screen)
	if(gl_FragColor.xyz != gl_FragColor.xyz)
		gl_FragColor.xyz = vec3(0.0);
#endif
}