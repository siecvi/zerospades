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

uniform mat4 projectionViewModelMatrix;
uniform mat4 viewModelMatrix;
uniform mat4 modelMatrix;
uniform mat4 modelNormalMatrix;
uniform vec3 modelOrigin;
uniform vec3 sunLightDirection;
uniform vec3 viewOriginVector;
uniform vec2 texScale;

// [x, y, z]
attribute vec3 positionAttribute;

// [u, v]
attribute vec2 textureCoordAttribute;

// [x, y, z]
attribute vec3 normalAttribute;

varying vec4 textureCoord;
varying vec3 fogDensity;
varying float flatShading;

void PrepareShadow(vec3 worldOrigin, vec3 normal);
vec4 ComputeFogDensity(float poweredLength);

void main() {
	vec4 vertexPos = vec4(positionAttribute + modelOrigin, 1.0);

	gl_Position = projectionViewModelMatrix * vertexPos;

	textureCoord = textureCoordAttribute.xyxy * vec4(texScale, vec2(1.0));
	
	// direct sunlight
	vec3 normal = normalize((modelNormalMatrix * vec4(normalAttribute, 1.0)).xyz);
	flatShading = max(dot(normal, sunLightDirection), 0.0);
	
	vec3 worldPosition = (modelMatrix * vertexPos).xyz;
	vec2 horzRelativePos = worldPosition.xy - viewOriginVector.xy;
	float horzDistance = dot(horzRelativePos, horzRelativePos);
	fogDensity = ComputeFogDensity(horzDistance).xyz;

	PrepareShadow(worldPosition, normal);
}